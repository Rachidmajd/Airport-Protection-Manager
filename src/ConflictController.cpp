#include "ConflictController.h"
#include <spdlog/spdlog.h>
#include "gdal.h"
#include "ogr_api.h"

#include "ogr_spatialref.h"
#include "ProjectRepository.h"
#include <mutex>
#include <memory>
#include <json.hpp>

namespace aeronautical {

// Static member definitions for singleton pattern
std::unique_ptr<ConflictController> ConflictController::instance_ = nullptr;
std::once_flag ConflictController::once_flag_;

ConflictController::ConflictController() 
    : repository_(std::make_unique<ConflictRepository>()) {
    GDALAllRegister();
}

// Singleton getInstance method
ConflictController& ConflictController::getInstance() {
    std::call_once(once_flag_, []() {
        instance_ = std::unique_ptr<ConflictController>(new ConflictController());
    });
    return *instance_;
}

void ConflictController::registerRoutes(crow::SimpleApp& app) {
    auto logger_ = spdlog::get("aeronautical");

    // GET /api/projects/:id/conflicts
    CROW_ROUTE(app, "/api/projects/<int>/conflicts")
        .methods(crow::HTTPMethod::GET)
        ([this](int project_id) {
            return getConflictsByProject(project_id);
        });

    logger_->info("Conflict routes registered");
}

crow::response ConflictController::getConflictsByProject(int project_id) {
    auto logger_ = spdlog::get("aeronautical");
    try {
        
        auto conflicts = repository_->findByProjectId(project_id);
        
        nlohmann::json response_data = nlohmann::json::array();
        for (const auto& conflict : conflicts) {
            response_data.push_back(conflict.toJson());
        }
        
        return crow::response(200, response_data.dump());

    } catch (const std::exception& e) {
        logger_->error("Failed to get conflicts for project {}: {}", project_id, e.what());
        return crow::response(500, "{\"error\":\"Internal server error\"}");
    }
}

// Helper function to create geometry from GeoJSON - FIXED to handle FeatureCollections
OGRGeometryH createGeometryFromGeoJSON(const std::string& geojson) {
    try {
        // Parse the JSON to check if it's a FeatureCollection
        nlohmann::json j = nlohmann::json::parse(geojson);
        
        if (j.contains("type") && j["type"] == "FeatureCollection") {
            // It's a FeatureCollection - we need to extract and merge geometries
            if (!j.contains("features") || !j["features"].is_array() || j["features"].empty()) {
                spdlog::error("FeatureCollection has no features");
                return nullptr;
            }
            
            OGRGeometryCollection* collection = new OGRGeometryCollection();
            
            for (const auto& feature : j["features"]) {
                if (!feature.contains("geometry")) continue;
                
                std::string geom_str = feature["geometry"].dump();
                OGRGeometry* geom = OGRGeometryFactory::createFromGeoJson(geom_str.c_str());
                
                if (geom) {
                    // Fix invalid geometries before adding
                    if (!geom->IsValid()) {
                        spdlog::warn("Fixing invalid geometry in FeatureCollection");
                        OGRGeometry* fixed = geom->Buffer(0); // Buffer(0) can fix self-intersections
                        if (fixed) {
                            delete geom;
                            geom = fixed;
                        }
                    }
                    collection->addGeometry(geom);
                    delete geom; // Collection takes ownership, but we need to clean up our pointer
                }
            }
            
            // If we only have one geometry, return it directly
            // Otherwise, return a union of all geometries
            if (collection->getNumGeometries() == 1) {
                OGRGeometry* single = collection->getGeometryRef(0)->clone();
                delete collection;
                return (OGRGeometryH)single;
            } else if (collection->getNumGeometries() > 1) {
                // Create a union of all geometries
                OGRGeometry* unionGeom = collection->UnionCascaded();
                delete collection;
                return (OGRGeometryH)unionGeom;
            } else {
                delete collection;
                return nullptr;
            }
        } else if (j.contains("type") && j["type"] == "Feature") {
            // It's a single Feature - extract the geometry
            if (!j.contains("geometry")) {
                spdlog::error("Feature has no geometry");
                return nullptr;
            }
            
            std::string geom_str = j["geometry"].dump();
            OGRGeometry* geometry = OGRGeometryFactory::createFromGeoJson(geom_str.c_str());
            
            // Fix invalid geometry
            if (geometry && !geometry->IsValid()) {
                spdlog::warn("Fixing invalid Feature geometry");
                OGRGeometry* fixed = geometry->Buffer(0);
                if (fixed) {
                    delete geometry;
                    geometry = fixed;
                }
            }
            
            return (OGRGeometryH)geometry;
        } else {
            // Try to parse it as a direct geometry
            OGRGeometry* geometry = OGRGeometryFactory::createFromGeoJson(geojson.c_str());
            
            // Fix invalid geometry
            if (geometry && !geometry->IsValid()) {
                spdlog::warn("Fixing invalid direct geometry");
                OGRGeometry* fixed = geometry->Buffer(0);
                if (fixed) {
                    delete geometry;
                    geometry = fixed;
                }
            }
            
            if (geometry) {
                return (OGRGeometryH)geometry;
            }
            
            // Fallback to C API
            OGRGeometryH hGeom = OGR_G_CreateGeometryFromJson(geojson.c_str());
            if (hGeom) {
                return hGeom;
            }
        }
    } catch (const std::exception& e) {
        spdlog::error("Exception while parsing GeoJSON: {}", e.what());
    }
    
    spdlog::error("Failed to create geometry from GeoJSON");
    return nullptr;
}

void ConflictController::analyzeProject(int project_id) {
    spdlog::info("Starting C++ conflict analysis for project ID: {}", project_id);

    // 1. Setup
    repository_->deleteByProjectId(project_id);
    ProjectRepository proj_repo;
    FlightProcedureRepository proc_repo;

    // 2. Fetch Geometries
    auto project_geom_json = proj_repo.findGeometriesByProjectId(project_id);
    auto all_protections = proc_repo.findAllActiveProtections();

    if (!project_geom_json || all_protections.empty()) {
        spdlog::warn("No project geometry or no protection zones found. Aborting analysis for project {}.", project_id);
        return;
    }

    // 3. Parse the project FeatureCollection
    std::vector<OGRGeometryH> project_geometries;
    
    try {
        nlohmann::json project_json = nlohmann::json::parse(*project_geom_json);
        
        if (project_json.contains("type") && project_json["type"] == "FeatureCollection") {
            if (!project_json.contains("features") || !project_json["features"].is_array()) {
                spdlog::error("Invalid FeatureCollection for project {}", project_id);
                return;
            }
            
            // Parse each feature individually
            for (size_t i = 0; i < project_json["features"].size(); i++) {
                const auto& feature = project_json["features"][i];
                
                if (!feature.contains("geometry")) {
                    spdlog::warn("Feature {} missing geometry, skipping", i);
                    continue;
                }
                
                std::string geom_str = feature["geometry"].dump();
                OGRGeometryH hGeom = createSimpleGeometryFromGeoJSON(geom_str);
                
                if (hGeom) {
                    project_geometries.push_back(hGeom);
                    spdlog::debug("Successfully parsed project geometry {} of type {}", 
                                i, ((OGRGeometry*)hGeom)->getGeometryName());
                } else {
                    spdlog::warn("Failed to parse project geometry {}", i);
                }
            }
        } else {
            // Handle single geometry
            OGRGeometryH hGeom = createSimpleGeometryFromGeoJSON(*project_geom_json);
            if (hGeom) {
                project_geometries.push_back(hGeom);
            }
        }
        
        if (project_geometries.empty()) {
            spdlog::error("No valid geometries found for project {}", project_id);
            return;
        }
        
        spdlog::info("Found {} valid geometries for project {}", project_geometries.size(), project_id);
        
    } catch (const std::exception& e) {
        spdlog::error("Exception parsing project geometries for project {}: {}", project_id, e.what());
        return;
    }

    int conflicts_found = 0;
    
    // 4. Test each project geometry against each protection zone
    for (const auto& protection : all_protections) {
        OGRGeometryH hProtGeom = createSimpleGeometryFromGeoJSON(protection.protection_geometry);
        if (!hProtGeom) {
            spdlog::warn("Could not parse protection geometry for procedure {}, skipping", protection.procedure_id);
            continue;
        }
        
        OGRGeometry* protection_geometry = (OGRGeometry*)hProtGeom;
        
        // Validate and fix protection geometry if needed
        if (!protection_geometry->IsValid()) {
            spdlog::warn("Protection geometry for procedure {} is invalid, attempting to fix", protection.procedure_id);
            OGRGeometry* fixed = protection_geometry->Buffer(0);
            if (fixed) {
                OGR_G_DestroyGeometry(hProtGeom);
                hProtGeom = (OGRGeometryH)fixed;
                protection_geometry = fixed;
            }
        }
        
        bool conflict_found = false;
        std::vector<OGRGeometryH> intersections;
        
        // Test intersection with each project geometry
        for (size_t i = 0; i < project_geometries.size(); i++) {
            OGRGeometry* project_geometry = (OGRGeometry*)project_geometries[i];
            
            try {
                if (project_geometry->Intersects(protection_geometry)) {
                    conflict_found = true;
                    
                    // Compute intersection for this specific geometry pair
                    OGRGeometryH hIntersection = OGR_G_Intersection(project_geometries[i], hProtGeom);
                    if (hIntersection) {
                        intersections.push_back(hIntersection);
                        spdlog::debug("Conflict found between project geometry {} and procedure {}", 
                                    i, protection.procedure_id);
                    }
                }
            } catch (const std::exception& e) {
                spdlog::error("Exception during intersection check between project geometry {} and procedure {}: {}", 
                            i, protection.procedure_id, e.what());
            }
        }
        
        // If any conflicts found, save them
        if (conflict_found) {
            conflicts_found++;
            
            // Combine all intersections into a single geometry collection for storage
            std::string intersection_json = "{}";
            
            if (!intersections.empty()) {
                try {
                    if (intersections.size() == 1) {
                        // Single intersection
                        char* json_str = OGR_G_ExportToJson(intersections[0]);
                        if (json_str) {
                            intersection_json = json_str;
                            CPLFree(json_str);
                        }
                    } else {
                        // Multiple intersections - create a GeometryCollection
                        OGRGeometryCollection* collection = new OGRGeometryCollection();
                        for (auto hInt : intersections) {
                            OGRGeometry* geom = ((OGRGeometry*)hInt)->clone();
                            collection->addGeometry(geom);
                            delete geom; // addGeometry takes ownership but we need to clean up our clone
                        }
                        
                        char* json_str = OGR_G_ExportToJson((OGRGeometryH)collection);
                        if (json_str) {
                            intersection_json = json_str;
                            CPLFree(json_str);
                        }
                        delete collection;
                    }
                } catch (const std::exception& e) {
                    spdlog::warn("Failed to export intersection geometry to JSON: {}", e.what());
                    intersection_json = "{}";
                }
            }
            
            // Clean up intersection geometries
            for (auto hInt : intersections) {
                OGR_G_DestroyGeometry(hInt);
            }
            
            std::string description = "Conflict with procedure " + std::to_string(protection.procedure_id) 
                                    + " in protection area '" + protection.protection_name + "'.";
            
            if (!repository_->create(project_id, protection.procedure_id, description, intersection_json)) {
                spdlog::error("Failed to save conflict to database for project {} and procedure {}", 
                             project_id, protection.procedure_id);
            } else {
                spdlog::info("Saved conflict for project {} with procedure {}", 
                            project_id, protection.procedure_id);
            }
        }
        
        OGR_G_DestroyGeometry(hProtGeom);
    }

    // Clean up project geometries
    for (auto hGeom : project_geometries) {
        OGR_G_DestroyGeometry(hGeom);
    }

    spdlog::info("C++ conflict analysis for project {} complete. Found {} conflicts.", project_id, conflicts_found);

    // Update project status
    auto projectToUpdateOpt = proj_repo.findById(project_id);
    if (projectToUpdateOpt) {
        Project projectToUpdate = *projectToUpdateOpt;
        projectToUpdate.status = ProjectStatus::UnderReview;
        
        if (proj_repo.update(project_id, projectToUpdate)) {
            spdlog::info("Successfully updated project {} status to UnderReview.", project_id);
        } else {
            spdlog::error("Failed to update project {} status after analysis.", project_id);
        }
    } else {
        spdlog::error("Could not find project {} to update its status after analysis.", project_id);
    }
}

// Simplified geometry creation function that avoids union operations
OGRGeometryH ConflictController::createSimpleGeometryFromGeoJSON(const std::string& geojson) {
    try {
        nlohmann::json j = nlohmann::json::parse(geojson);
        
        if (j.contains("type") && j["type"] == "Feature") {
            if (!j.contains("geometry")) {
                spdlog::error("Feature has no geometry");
                return nullptr;
            }
            std::string geom_str = j["geometry"].dump();
            return createSimpleGeometryFromGeoJSON(geom_str);
        }
        
        // Direct geometry parsing
        OGRGeometry* geometry = OGRGeometryFactory::createFromGeoJson(geojson.c_str());
        
        if (geometry && !geometry->IsValid()) {
            spdlog::warn("Fixing invalid geometry");
            OGRGeometry* fixed = geometry->Buffer(0);
            if (fixed) {
                delete geometry;
                geometry = fixed;
            }
        }
        
        return (OGRGeometryH)geometry;
        
    } catch (const std::exception& e) {
        spdlog::error("Exception while parsing GeoJSON: {}", e.what());
        return nullptr;
    }
}
} // namespace aeronautical