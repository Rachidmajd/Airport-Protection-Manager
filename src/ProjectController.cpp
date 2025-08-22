#include "ProjectController.h"
#include <json.hpp>
#include <spdlog/sinks/stdout_color_sinks.h>
#include "DatabaseManager.h"


namespace aeronautical {

ProjectController::ProjectController() 
    : repository_(std::make_unique<ProjectRepository>()) {
    // Fixed spdlog initialization
    try {
        logger_ = spdlog::get("aeronautical");
        if (!logger_) {
            // Create a console logger with color support
            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            logger_ = std::make_shared<spdlog::logger>("aeronautical", console_sink);
            spdlog::register_logger(logger_);
        }
    } catch (const std::exception& e) {
        // Fallback to default logger
        logger_ = spdlog::default_logger();
    }
}

void ProjectController::registerRoutes(crow::SimpleApp& app) {
    // GET /api/projects
    CROW_ROUTE(app, "/api/projects")
        .methods(crow::HTTPMethod::GET)
        ([this](const crow::request& req) {
            return getProjects(req);
        });
    
    // GET /api/projects/:id
    CROW_ROUTE(app, "/api/projects/<int>")
        .methods(crow::HTTPMethod::GET)
        ([this](int id) {
            return getProject(id);
        });
    
    // GET /api/projects/code/:code
    CROW_ROUTE(app, "/api/projects/code/<string>")
        .methods(crow::HTTPMethod::GET)
        ([this](const std::string& code) {
            return getProjectByCode(code);
        });
    
    // POST /api/projects
    CROW_ROUTE(app, "/api/projects")
        .methods(crow::HTTPMethod::POST)
        ([this](const crow::request& req) {
            return createProject(req);
        });
    
    // PUT /api/projects/:id
    CROW_ROUTE(app, "/api/projects/<int>")
        .methods(crow::HTTPMethod::PUT)
        ([this](const crow::request& req, int id) {
            return updateProject(id, req);
        });
    
    // DELETE /api/projects/:id
    CROW_ROUTE(app, "/api/projects/<int>")
        .methods(crow::HTTPMethod::DELETE)
        ([this](int id) {
            return deleteProject(id);
        });

        // POST /api/projects/:id/submit
    CROW_ROUTE(app, "/api/projects/<int>/submit")
        .methods(crow::HTTPMethod::POST)
        ([this](const crow::request& req, int id) {
            return submitProject(id, req);
        });
    
        // GET /api/projects/:id/geometries
    CROW_ROUTE(app, "/api/projects/<int>/geometries")
        .methods(crow::HTTPMethod::GET)
        ([this](int id) {
            return getProjectGeometries(id);
        });

    logger_->info("Project routes registered");
}

crow::response ProjectController::getProjects(const crow::request& req) {
    try {
        ProjectFilter filter;
        
        // Parse query parameters
        auto query = crow::query_string(req.url_params);
        
        if (query.get("status")) {
            filter.status = stringToStatus(query.get("status"));
        }
        
        if (query.get("demander_id")) {
            filter.demander_id = std::stoi(query.get("demander_id"));
        }
        
        if (query.get("priority")) {
            filter.priority = stringToPriority(query.get("priority"));
        }
        
        if (query.get("limit")) {
            filter.limit = std::stoi(query.get("limit"));
            if (filter.limit > 500) filter.limit = 500; // Max limit
        }
        
        if (query.get("offset")) {
            filter.offset = std::stoi(query.get("offset"));
        }
        
        auto projects = repository_->findAll(filter);
        int total = repository_->count(filter);
        
        nlohmann::json response;
        response["data"] = nlohmann::json::array();
        for (const auto& project : projects) {
            response["data"].push_back(project.toJson());
        }
        response["total"] = total;
        response["limit"] = filter.limit;
        response["offset"] = filter.offset;
        
        return successResponse(response);
        
    } catch (const std::exception& e) {
        logger_->error("Failed to get projects: {}", e.what());
        return errorResponse(500, "Internal server error");
    }
}

crow::response ProjectController::getProject(int id) {
    try {
        auto project = repository_->findById(id);
        
        if (!project) {
            return errorResponse(404, "Project not found");
        }
        
        nlohmann::json response;
        response["data"] = project->toJson();
        
        return successResponse(response);
        
    } catch (const std::exception& e) {
        logger_->error("Failed to get project {}: {}", id, e.what());
        return errorResponse(500, "Internal server error");
    }
}

crow::response ProjectController::getProjectByCode(const std::string& code) {
    try {
        auto project = repository_->findByCode(code);
        
        if (!project) {
            return errorResponse(404, "Project not found");
        }
        
        nlohmann::json response;
        response["data"] = project->toJson();
        
        return successResponse(response);
        
    } catch (const std::exception& e) {
        logger_->error("Failed to get project by code {}: {}", code, e.what());
        return errorResponse(500, "Internal server error");
    }
}

crow::response ProjectController::getProjectGeometries(int project_id) {
    try {
        auto geometry_json_str = repository_->findGeometriesByProjectId(project_id);
        
        if (geometry_json_str) {
            // If geometry is found, return it as JSON
            auto json_response = nlohmann::json::parse(*geometry_json_str);
            return successResponse(json_response);
        } else {
            // If no geometry is found, return an empty FeatureCollection
            nlohmann::json empty_collection = {
                {"type", "FeatureCollection"},
                {"features", nlohmann::json::array()}
            };
            return successResponse(empty_collection);
        }
        
    } catch (const std::exception& e) {
        logger_->error("Failed to get geometries for project {}: {}", project_id, e.what());
        return errorResponse(500, "Internal server error");
    }
}

crow::response ProjectController::createProject(const crow::request& req) {
    try {
        // Check authorization
        std::string authError;
        if (!checkAuthorization(req, authError)) {
            return errorResponse(401, authError);
        }
        
        // Parse request body
        auto body = nlohmann::json::parse(req.body);
        
        // Validate input
        std::string validationError;
        if (!validateProjectInput(body, validationError)) {
            return errorResponse(400, validationError);
        }
        
        // Create project from JSON
        Project project = Project::fromJson(body);
        
        // Save to database
        auto created = repository_->create(project);
        
        nlohmann::json response;
        response["data"] = created.toJson();
        response["message"] = "Project created successfully";
        
        logger_->info("Created project: {} - {}", created.project_code, created.title);
        
        return crow::response(201, response.dump());
        
    } catch (const nlohmann::json::exception& e) {
        logger_->error("Invalid JSON in create project request: {}", e.what());
        return errorResponse(400, "Invalid JSON format");
    } catch (const std::exception& e) {
        logger_->error("Failed to create project: {}", e.what());
        return errorResponse(500, "Internal server error");
    }
}

crow::response ProjectController::updateProject(int id, const crow::request& req) {
    try {
        // Check authorization
        std::string authError;
        if (!checkAuthorization(req, authError)) {
            return errorResponse(401, authError);
        }
        
        // Check if project exists
        auto existing = repository_->findById(id);
        if (!existing) {
            return errorResponse(404, "Project not found");
        }
        
        // Parse request body
        auto body = nlohmann::json::parse(req.body);
        
        // Validate input
        std::string validationError;
        if (!validateProjectInput(body, validationError)) {
            return errorResponse(400, validationError);
        }
        
        // Update project from JSON
        Project project = Project::fromJson(body);
        project.id = id; // Ensure ID is not changed
        project.project_code = existing->project_code; // Code cannot be changed
        
        // Save to database
        bool updated = repository_->update(id, project);
        
        if (!updated) {
            return errorResponse(500, "Failed to update project");
        }
        
        // Get updated project
        auto updatedProject = repository_->findById(id);
        
        nlohmann::json response;
        response["data"] = updatedProject->toJson();
        response["message"] = "Project updated successfully";
        
        logger_->info("Updated project: {} - {}", updatedProject->project_code, updatedProject->title);
        
        return successResponse(response);
        
    } catch (const nlohmann::json::exception& e) {
        logger_->error("Invalid JSON in update project request: {}", e.what());
        return errorResponse(400, "Invalid JSON format");
    } catch (const std::exception& e) {
        logger_->error("Failed to update project {}: {}", id, e.what());
        return errorResponse(500, "Internal server error");
    }
}

crow::response ProjectController::deleteProject(int id) {
    try {
        // Check authorization
        std::string authError;
        if (!checkAuthorization(crow::request(), authError)) {
            return errorResponse(401, authError);
        }
        
        bool deleted = repository_->deleteById(id);
        
        if (!deleted) {
            return errorResponse(404, "Project not found");
        }
        
        nlohmann::json response;
        response["message"] = "Project deleted successfully";
        
        logger_->info("Deleted project with ID: {}", id);
        
        return successResponse(response);
        
    } catch (const std::exception& e) {
        logger_->error("Failed to delete project {}: {}", id, e.what());
        return errorResponse(500, "Internal server error");
    }
}

crow::response ProjectController::submitProject(int id, const crow::request& req) {
    try {
        // Check authorization
        std::string authError;
        if (!checkAuthorization(req, authError)) {
            return errorResponse(401, authError);
        }
        
        // Parse request body
        auto body = nlohmann::json::parse(req.body);
        
        // Check if project exists
        auto project = repository_->findById(id);
        if (!project) {
            return errorResponse(404, "Project not found");
        }
        
        // Validate and save GeoJSON if provided
        if (body.contains("geometry") && !body["geometry"].is_null()) {
            std::string geometryError;
            if (!validateGeoJSON(body["geometry"], geometryError)) {
                return errorResponse(400, "Invalid GeoJSON: " + geometryError);
            }
            
            // 👇 REPLACE the old saveProjectGeometry call with the new one
            if (!saveOrUpdateProjectGeometryCollection(id, body["geometry"])) {
                return errorResponse(500, "Failed to save project geometry");
            }
        }
        
        // Update project status to Pending
        project->status = ProjectStatus::Pending;
        project->updated_at = std::chrono::system_clock::now();


        //dumping the body for debug purposes : this should be removed for production
        logger_->info("Received geometry payload for project ID {}:\n{}", id, body.dump(2));

        
        // Save the updated project
        bool updated = repository_->update(id, *project);
        
        if (!updated) {
            return errorResponse(500, "Failed to submit project");
        }

        // ✨ LAUNCH CONFLICT DETECTION IN THE BACKGROUND
        logger_->info("Launching background conflict analysis for project ID: {}", id);
        std::async(std::launch::async, &ConflictController::analyzeProject, &ConflictController::getInstance(), id);        
        
        // Add project comment for status change
        addProjectComment(id, "Project submitted for review", ProjectStatus::Created, ProjectStatus::Pending);
        
         // 3. Immediately return a 202 Accepted response
        // This tells the client the request was accepted and is being processed.
        nlohmann::json response;
        response["message"] = "Project submission accepted. Analysis is in progress.";
        response["data"] = project->toJson(); // Return the project in its "Pending" state
        
        // Use status code 202 for "Accepted"
        crow::response res(202, response.dump());
        res.add_header("Content-Type", "application/json");
        return res;
        
    } catch (const nlohmann::json::exception& e) {
        logger_->error("Invalid JSON in submit project request: {}", e.what());
        return errorResponse(400, "Invalid JSON format");
    } catch (const std::exception& e) {
        logger_->error("Failed to submit project {}: {}", id, e.what());
        return errorResponse(500, "Internal server error");
    }
}

bool ProjectController::saveOrUpdateProjectGeometryCollection(int project_id, const nlohmann::json& incoming_geojson) {
    if (!incoming_geojson.contains("features") || !incoming_geojson["features"].is_array()) {
        logger_->error("Incoming GeoJSON for project {} is not a valid FeatureCollection", project_id);
        return false;
    }

    try {
        auto& db = DatabaseManager::getInstance();
        nlohmann::json final_collection;

        // 1. Check if a geometry collection already exists for this project
        std::string select_query = "SELECT geometry_data FROM project_geometries WHERE project_id = " + std::to_string(project_id) + " AND is_primary = 1";
        MYSQL_RES* result = db.executeSelectQuery(select_query);
        
        if (result && mysql_num_rows(result) > 0) {
            MYSQL_ROW row = mysql_fetch_row(result);
            if (row && row[0]) {
                // An existing collection was found, parse it
                final_collection = nlohmann::json::parse(row[0]);
                logger_->debug("Found existing geometry collection for project {}. Merging.", project_id);
            }
            mysql_free_result(result);
        }

        // 2. Merge new features into the collection
        if (!final_collection.contains("type") || final_collection["type"] != "FeatureCollection") {
            // If no existing collection, create a new one
            final_collection = nlohmann::json::object({
                {"type", "FeatureCollection"},
                {"features", nlohmann::json::array()}
            });
        }

        // Add all new features from the incoming request
        for (const auto& feature : incoming_geojson["features"]) {
            final_collection["features"].push_back(feature);
        }
        
        // 3. Prepare the data for an UPSERT operation
        std::string geo_json_string = final_collection.dump();
        std::string escaped_json = geo_json_string;
        
        // Basic escaping for SQL
        size_t pos = 0;
        while ((pos = escaped_json.find("'", pos)) != std::string::npos) {
            escaped_json.replace(pos, 1, "\\'");
            pos += 2;
        }

        std::string project_name = "Aggregated Project Geometry"; // Default name
        
        // 4. Execute the UPSERT query
        std::stringstream query;
        query << "INSERT INTO project_geometries (project_id, name, geometry_data, is_primary, geometry_type) "
              << "VALUES (" << project_id << ", '" << project_name << "', '" << escaped_json << "', 1, 'collection') "
              << "ON DUPLICATE KEY UPDATE "
              << "geometry_data = VALUES(geometry_data), "
              << "updated_at = NOW()";

        if (!db.executeQuery(query.str())) {
            logger_->error("Failed to upsert geometry collection for project {}", project_id);
            return false;
        }

        logger_->info("Successfully saved/updated geometry collection for project {}", project_id);
        return true;

    } catch (const std::exception& e) {
        logger_->error("Exception in saveOrUpdateProjectGeometryCollection for project {}: {}", project_id, e.what());
        return false;
    }
}


bool ProjectController::validateGeoJSON(const nlohmann::json& geojson, std::string& error) {
    // Basic GeoJSON validation
    if (!geojson.contains("type")) {
        error = "GeoJSON must have a 'type' field";
        return false;
    }
    
    std::string type = geojson["type"];
    
    // Check if it's a Feature or FeatureCollection
    if (type == "Feature") {
        if (!geojson.contains("geometry")) {
            error = "Feature must have a 'geometry' field";
            return false;
        }
        return validateGeometry(geojson["geometry"], error);
    } else if (type == "FeatureCollection") {
        if (!geojson.contains("features")) {
            error = "FeatureCollection must have a 'features' field";
            return false;
        }
        for (const auto& feature : geojson["features"]) {
            if (!validateGeoJSON(feature, error)) {
                return false;
            }
        }
        return true;
    } else {
        // It's a direct geometry
        return validateGeometry(geojson, error);
    }
}

bool ProjectController::validateGeometry(const nlohmann::json& geometry, std::string& error) {
    if (!geometry.contains("type")) {
        error = "Geometry must have a 'type' field";
        return false;
    }
    
    if (!geometry.contains("coordinates")) {
        error = "Geometry must have a 'coordinates' field";
        return false;
    }
    
    std::string type = geometry["type"];
    std::vector<std::string> validTypes = {
        "Point", "LineString", "Polygon", 
        "MultiPoint", "MultiLineString", "MultiPolygon", 
        "GeometryCollection"
    };
    
    if (std::find(validTypes.begin(), validTypes.end(), type) == validTypes.end()) {
        error = "Invalid geometry type: " + type;
        return false;
    }
    
    // TODO: Add more detailed coordinate validation based on geometry type
    
    return true;
}

bool ProjectController::saveProjectGeometry(int project_id, const nlohmann::json& geojson) {
    try {
        auto& db = DatabaseManager::getInstance();
        
        // Get project altitude ranges for defaults
        int defaultAltMin = 0, defaultAltMax = 400;
        auto project = repository_->findById(project_id);
        if (project) {
            if (project->altitude_min) defaultAltMin = *project->altitude_min;
            if (project->altitude_max) defaultAltMax = *project->altitude_max;
        }
        
        // Check if it's a FeatureCollection with multiple features
        if (geojson.contains("type") && geojson["type"] == "FeatureCollection") {
            if (!geojson.contains("features") || !geojson["features"].is_array()) {
                logger_->error("FeatureCollection missing features array");
                return false;
            }
            
            // Clear any existing geometries for this project (optional)
            // Uncomment if you want to replace existing geometries
            // std::string deleteQuery = "DELETE FROM project_geometries WHERE project_id = " + std::to_string(project_id);
            // db.executeQuery(deleteQuery);
            
            int featureIndex = 0;
            bool hasPrimary = false;
            
            // Process each feature separately
            for (const auto& feature : geojson["features"]) {
                if (!feature.contains("geometry")) {
                    logger_->warn("Feature {} missing geometry, skipping", featureIndex);
                    continue;
                }
                
                // Set first geometry as primary if none marked
                bool isPrimary = (featureIndex == 0 && !hasPrimary);
                
                // Check if explicitly marked as primary
                if (feature.contains("properties") && 
                    feature["properties"].contains("is_primary") && 
                    feature["properties"]["is_primary"].get<bool>()) {
                    isPrimary = true;
                    hasPrimary = true;
                }
                
                if (!saveIndividualGeometry(project_id, feature, defaultAltMin, defaultAltMax, isPrimary)) {
                    logger_->error("Failed to save feature {} for project {}", featureIndex, project_id);
                    // Continue with other features even if one fails
                }
                
                featureIndex++;
            }
            
            logger_->info("Saved {} geometries for project {}", featureIndex, project_id);
            return featureIndex > 0; // Success if at least one geometry was saved
            
        } else if (geojson.contains("type") && geojson["type"] == "Feature") {
            // Single feature
            return saveIndividualGeometry(project_id, geojson, defaultAltMin, defaultAltMax, true);
            
        } else if (geojson.contains("type") && geojson.contains("coordinates")) {
            // Direct geometry (wrap in a feature)
            nlohmann::json feature;
            feature["type"] = "Feature";
            feature["geometry"] = geojson;
            feature["properties"] = nlohmann::json::object();
            return saveIndividualGeometry(project_id, feature, defaultAltMin, defaultAltMax, true);
            
        } else {
            logger_->error("Invalid GeoJSON structure");
            return false;
        }
        
    } catch (const std::exception& e) {
        logger_->error("Failed to save project geometry: {}", e.what());
        return false;
    }
}

bool ProjectController::saveIndividualGeometry(int project_id, const nlohmann::json& feature, 
                                              int defaultAltMin, int defaultAltMax, bool isPrimary) {
    try {
        auto& db = DatabaseManager::getInstance();
        
        // Extract geometry
        nlohmann::json geometry = feature["geometry"];
        std::string geoType = geometry["type"];
        
        // Default values
        std::string geometryType = "operational_area";
        std::string geometryName = "Operation Area";
        std::string description = "";
        int altMin = defaultAltMin;
        int altMax = defaultAltMax;
        
        // Extract properties if available
        if (feature.contains("properties") && feature["properties"].is_object()) {
            auto props = feature["properties"];
            
            if (props.contains("name") && props["name"].is_string()) {
                geometryName = props["name"];
            }
            
            if (props.contains("description") && props["description"].is_string()) {
                description = props["description"];
            }
            
            if (props.contains("geometry_type") && props["geometry_type"].is_string()) {
                std::string propType = props["geometry_type"];
                // Map to your enum values
                if (propType == "no_fly_zone") geometryType = "no_fly_zone";
                else if (propType == "buffer_zone") geometryType = "buffer_zone";
                else if (propType == "waypoint") geometryType = "waypoint";
                else if (propType == "operational_area") geometryType = "operational_area";
            }
            
            if (props.contains("altitude_min") && props["altitude_min"].is_number()) {
                altMin = props["altitude_min"];
            }
            
            if (props.contains("altitude_max") && props["altitude_max"].is_number()) {
                altMax = props["altitude_max"];
            }
        }
        
        // Calculate center point and area based on geometry type
        double centerLat = 0, centerLng = 0;
        double areaSize = 0;
        
        if (geoType == "Point" && geometry.contains("coordinates")) {
            auto coords = geometry["coordinates"];
            if (coords.size() >= 2) {
                centerLng = coords[0].get<double>();
                centerLat = coords[1].get<double>();
            }
        } else if (geoType == "Polygon" && geometry.contains("coordinates")) {
            // Calculate centroid for polygon
            auto outerRing = geometry["coordinates"][0];
            double sumLat = 0, sumLng = 0;
            int count = 0;
            
            for (const auto& point : outerRing) {
                if (point.size() >= 2) {
                    sumLng += point[0].get<double>();
                    sumLat += point[1].get<double>();
                    count++;
                }
            }
            
            if (count > 0) {
                centerLat = sumLat / count;
                centerLng = sumLng / count;
            }
            
            // Calculate approximate area (simplified)
            areaSize = calculatePolygonArea(outerRing);
            
        } else if (geoType == "LineString" && geometry.contains("coordinates")) {
            // Calculate midpoint for linestring
            auto coords = geometry["coordinates"];
            if (coords.size() > 0) {
                size_t midIndex = coords.size() / 2;
                if (coords[midIndex].size() >= 2) {
                    centerLng = coords[midIndex][0].get<double>();
                    centerLat = coords[midIndex][1].get<double>();
                }
            }
        } else if (geoType == "MultiPolygon" && geometry.contains("coordinates")) {
            // For MultiPolygon, calculate center of the first polygon
            if (geometry["coordinates"].size() > 0 && geometry["coordinates"][0].size() > 0) {
                auto firstPolygon = geometry["coordinates"][0][0];
                double sumLat = 0, sumLng = 0;
                int count = 0;
                
                for (const auto& point : firstPolygon) {
                    if (point.size() >= 2) {
                        sumLng += point[0].get<double>();
                        sumLat += point[1].get<double>();
                        count++;
                    }
                }
                
                if (count > 0) {
                    centerLat = sumLat / count;
                    centerLng = sumLng / count;
                }
            }
        }
        
        // Prepare geometry data as JSON string
        std::string jsonStr = geometry.dump();
        
        // Escape single quotes for SQL
        size_t pos = 0;
        while ((pos = jsonStr.find("'", pos)) != std::string::npos) {
            jsonStr.replace(pos, 1, "\\'");
            pos += 2;
        }
        
        // Escape other strings
        std::string escapedName = geometryName;
        pos = 0;
        while ((pos = escapedName.find("'", pos)) != std::string::npos) {
            escapedName.replace(pos, 1, "\\'");
            pos += 2;
        }
        
        std::string escapedDesc = description;
        pos = 0;
        while ((pos = escapedDesc.find("'", pos)) != std::string::npos) {
            escapedDesc.replace(pos, 1, "\\'");
            pos += 2;
        }
        
        // Build insert query
        std::stringstream query;
        query << "INSERT INTO project_geometries "
              << "(project_id, geometry_type, name, description, geometry_data, "
              << "center_lat, center_lng, area_size, altitude_min, altitude_max, is_primary) "
              << "VALUES ("
              << project_id << ", "
              << "'" << geometryType << "', "
              << "'" << escapedName << "', ";
        
        if (!escapedDesc.empty()) {
            query << "'" << escapedDesc << "', ";
        } else {
            query << "NULL, ";
        }
        
        query << "'" << jsonStr << "', ";
        
        if (centerLat != 0 && centerLng != 0) {
            query << centerLat << ", " << centerLng << ", ";
        } else {
            query << "NULL, NULL, ";
        }
        
        if (areaSize > 0) {
            query << areaSize << ", ";
        } else {
            query << "NULL, ";
        }
        
        query << altMin << ", "
              << altMax << ", "
              << (isPrimary ? "1" : "0") << ")";
        
        bool result = db.executeQuery(query.str());
        
        if (result) {
            logger_->debug("Saved geometry: type={}, name={}, geometry_type={}, primary={}", 
                         geoType, geometryName, geometryType, isPrimary);
        }
        
        return result;
        
    } catch (const std::exception& e) {
        logger_->error("Failed to save individual geometry: {}", e.what());
        return false;
    }
}

double ProjectController::calculatePolygonArea(const nlohmann::json& coordinates) {
    // Simplified area calculation using Shoelace formula
    // For accurate results, you should use a proper GIS library
    double area = 0;
    size_t n = coordinates.size();
    
    if (n < 3) return 0; // Not a valid polygon
    
    for (size_t i = 0; i < n - 1; i++) {
        if (coordinates[i].size() >= 2 && coordinates[i + 1].size() >= 2) {
            double x1 = coordinates[i][0].get<double>();
            double y1 = coordinates[i][1].get<double>();
            double x2 = coordinates[i + 1][0].get<double>();
            double y2 = coordinates[i + 1][1].get<double>();
            
            area += (x1 * y2 - x2 * y1);
        }
    }
    
    // Convert from degrees to approximate square meters (very rough estimate)
    // At equator: 1 degree ≈ 111,000 meters
    area = std::abs(area / 2.0) * 111000 * 111000;
    
    return area;
}

void ProjectController::addProjectComment(int project_id, const std::string& comment, 
                                         ProjectStatus oldStatus, ProjectStatus newStatus) {
    try {
        auto& db = DatabaseManager::getInstance();
        
        // For now, use a default user_id (1) - you should get this from the authenticated user
        int user_id = 1; // TODO: Get from JWT token
        
        std::stringstream query;
        query << "INSERT INTO project_comments "
              << "(project_id, user_id, comment_type, comment, old_status, new_status, is_internal) "
              << "VALUES ("
              << project_id << ", "
              << user_id << ", "
              << "'status_change', "
              << "'" << comment << "', "
              << "'" << statusToString(oldStatus) << "', "
              << "'" << statusToString(newStatus) << "', "
              << "0)"; // Not internal
        
        db.executeQuery(query.str());
        
    } catch (const std::exception& e) {
        logger_->warn("Failed to add project comment: {}", e.what());
        // Don't fail the whole operation if comment fails
    }
}

crow::response ProjectController::errorResponse(int code, const std::string& message) {
    nlohmann::json response;
    response["error"] = true;
    response["message"] = message;
    
    crow::response res(code, response.dump());
    res.add_header("Content-Type", "application/json");
    return res;
}

crow::response ProjectController::successResponse(const nlohmann::json& data) {
    crow::response res(200, data.dump());
    res.add_header("Content-Type", "application/json");
    return res;
}

bool ProjectController::validateProjectInput(const nlohmann::json& input, std::string& error) {
    // Required fields
    if (!input.contains("title") || input["title"].get<std::string>().empty()) {
        error = "Title is required";
        return false;
    }
    
    if (!input.contains("demander_name") || input["demander_name"].get<std::string>().empty()) {
        error = "Demander name is required";
        return false;
    }
    
    if (!input.contains("demander_email") || input["demander_email"].get<std::string>().empty()) {
        error = "Demander email is required";
        return false;
    }
    
    // Validate email format (basic check)
    std::string email = input["demander_email"];
    if (email.find('@') == std::string::npos || email.find('.') == std::string::npos) {
        error = "Invalid email format";
        return false;
    }
    
    // Validate altitude ranges if provided
    if (input.contains("altitude_min") && input.contains("altitude_max")) {
        int min = input["altitude_min"].get<int>();
        int max = input["altitude_max"].get<int>();
        
        if (min < 0 || max < 0) {
            error = "Altitude values must be positive";
            return false;
        }
        
        if (min > max) {
            error = "Minimum altitude cannot be greater than maximum altitude";
            return false;
        }
        
        if (max > 400) {
            error = "Maximum altitude for drone operations is 400 feet AGL";
            return false;
        }
    }
    
    // Validate dates if provided
    if (input.contains("start_date") && input.contains("end_date")) {
        try {
            auto start = stringToTimePoint(input["start_date"]);
            auto end = stringToTimePoint(input["end_date"]);
            
            if (start > end) {
                error = "Start date must be before end date";
                return false;
            }
        } catch (...) {
            error = "Invalid date format. Use YYYY-MM-DD HH:MM:SS";
            return false;
        }
    }
    
    return true;
}

bool ProjectController::checkAuthorization(const crow::request& req, std::string& error) {
    // TODO: Implement proper JWT token validation
    // For now, just check for presence of Authorization header
    
    auto auth_header = req.get_header_value("Authorization");
    if (auth_header.empty()) {
        error = "Authorization header required";
        return false;
    }
    
    // Fixed: Use C++11/14 compatible substring check instead of starts_with (C++20)
    if (auth_header.length() < 7 || auth_header.substr(0, 7) != "Bearer ") {
        error = "Invalid authorization format";
        return false;
    }
    
    // TODO: Validate JWT token
    // For now, accept any bearer token
    
    return true;
}

} // namespace aeronautical
