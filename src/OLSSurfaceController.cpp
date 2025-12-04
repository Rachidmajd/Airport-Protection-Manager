#include "OLSSurfaceController.h"
#include <spdlog/sinks/stdout_color_sinks.h>

namespace aeronautical {

OLSSurfaceController::OLSSurfaceController() {
    try {
        logger_ = spdlog::get("aeronautical");
        if (!logger_) {
            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            logger_ = std::make_shared<spdlog::logger>("aeronautical", console_sink);
            spdlog::register_logger(logger_);
        }
    } catch (const std::exception& e) {
        logger_ = spdlog::default_logger();
    }
}

void OLSSurfaceController::registerRoutes(crow::SimpleApp& app) {
    // GET /api/ols/runway/:airport_icao/:runway_ident
    // Generate OLS for a specific runway
    CROW_ROUTE(app, "/api/ols/runway/<string>/<string>")
        .methods(crow::HTTPMethod::GET)
        ([this](const crow::request& req, const std::string& airport_icao, 
                const std::string& runway_ident) {
            return getOLSForRunway(req, airport_icao, runway_ident);
        });
    
    // GET /api/ols/airport/:airport_icao
    // Generate OLS for all runways at an airport
    CROW_ROUTE(app, "/api/ols/airport/<string>")
        .methods(crow::HTTPMethod::GET)
        ([this](const crow::request& req, const std::string& airport_icao) {
            return getOLSForAirport(req, airport_icao);
        });
    
    // POST /api/ols/check-penetration
    // Check if obstacles penetrate OLS
    CROW_ROUTE(app, "/api/ols/check-penetration")
        .methods(crow::HTTPMethod::POST)
        ([this](const crow::request& req) {
            return checkPenetration(req);
        });
    
    // GET /api/ols/parameters
    // Get OLS parameters for a given runway classification
    CROW_ROUTE(app, "/api/ols/parameters")
        .methods(crow::HTTPMethod::GET)
        ([this](const crow::request& req) {
            return getOLSParameters(req);
        });
    
    logger_->info("OLS Surface routes registered");
}

crow::response OLSSurfaceController::getOLSForRunway(
    const crow::request& req,
    const std::string& airport_icao,
    const std::string& runway_ident) {
    
    try {
        logger_->info("Generating OLS for runway {} at {}", runway_ident, airport_icao);
        
        // Parse optional query parameters
        ApproachCategory approach_category = ApproachCategory::NonPrecisionApproach;
        const char* cat_param = req.url_params.get("approach_category");
        if (cat_param) {
            approach_category = parseApproachCategory(cat_param);
        }
        
        bool include_3d = false;
        const char* threeDParam = req.url_params.get("3d");
        if (threeDParam && std::string(threeDParam) == "true") {
            include_3d = true;
        }
        
        // Get airport data
        Airport airport;
        try {
            airport = airportRepository_.fetchAirportByIcao(airport_icao);
        } catch (const std::exception& e) {
            return errorResponse(404, "Airport not found: " + airport_icao);
        }
        
        // For now, create runway data from the runway identifier
        // In production, you'd fetch this from AirportRepository
        // This is a simplified example
        
        RunwayEndData runway_end;
        runway_end.designator = runway_ident;
        
        // Parse runway heading from designator (e.g., "09" = 90°, "27L" = 270°)
        int runway_number = 0;
        try {
            // Extract numeric part
            std::string numeric_part;
            for (char c : runway_ident) {
                if (std::isdigit(c)) {
                    numeric_part += c;
                }
            }
            runway_number = std::stoi(numeric_part);
        } catch (...) {
            return errorResponse(400, "Invalid runway designator format");
        }
        
        runway_end.heading_deg = runway_number * 10.0;
        runway_end.threshold_lat = airport.latitude;
        runway_end.threshold_lon = airport.longitude;
        runway_end.threshold_elevation_m = airport.elevation_ft * 0.3048;
        
        // Default runway dimensions (should come from database)
        runway_end.runway_length_m = airport.longest_runway_ft * 0.3048;
        runway_end.runway_width_m = 45;  // Default for Code 3/4
        
        runway_end.code_number = OLSSurfaceGenerator::determineCodeNumber(runway_end.runway_length_m);
        runway_end.code_letter = OLSSurfaceGenerator::determineCodeLetter(runway_end.runway_width_m);
        runway_end.approach_category = approach_category;
        
        // Set strip dimensions based on code
        if (static_cast<int>(runway_end.code_number) >= 3) {
            runway_end.strip_length_beyond_end_m = 60;
            runway_end.strip_width_m = 150;
        } else {
            runway_end.strip_length_beyond_end_m = 30;
            runway_end.strip_width_m = 75;
        }
        
        // Generate OLS surfaces
        RunwayOLSSurfaces surfaces = generator_.generateSurfaces(runway_end);
        surfaces.airport_icao = airport_icao;
        
        // Convert to GeoJSON
        nlohmann::json response = surfaces.toGeoJSON();
        
        // Add metadata
        response["metadata"] = {
            {"airport_icao", airport_icao},
            {"runway_designator", runway_ident},
            {"approach_category", static_cast<int>(approach_category)},
            {"code_number", static_cast<int>(runway_end.code_number)},
            {"code_letter", static_cast<int>(runway_end.code_letter)},
            {"runway_length_m", runway_end.runway_length_m},
            {"generated_at", std::time(nullptr)}
        };
        
        logger_->info("Generated {} OLS features for {} runway {}", 
                     response["features"].size(), airport_icao, runway_ident);
        
        return successResponse(response);
        
    } catch (const std::exception& e) {
        logger_->error("Error generating OLS for runway: {}", e.what());
        return errorResponse(500, "Internal server error: " + std::string(e.what()));
    }
}

crow::response OLSSurfaceController::getOLSForAirport(
    const crow::request& req,
    const std::string& airport_icao) {
    
    try {
        logger_->info("Generating OLS for all runways at {}", airport_icao);
        
        // Get airport data
        Airport airport;
        try {
            airport = airportRepository_.fetchAirportByIcao(airport_icao);
        } catch (const std::exception& e) {
            return errorResponse(404, "Airport not found: " + airport_icao);
        }
        
        // Parse optional parameters
        ApproachCategory default_category = ApproachCategory::NonPrecisionApproach;
        const char* cat_param = req.url_params.get("approach_category");
        if (cat_param) {
            default_category = parseApproachCategory(cat_param);
        }
        
        // Get runways for this airport
        // Note: You may need to implement fetchRunwaysByAirport in AirportRepository
        // For now, we'll create a placeholder response
        
        nlohmann::json response;
        response["type"] = "FeatureCollection";
        response["features"] = nlohmann::json::array();
        response["airport"] = {
            {"icao", airport.icao_code},
            {"name", airport.name},
            {"elevation_ft", airport.elevation_ft},
            {"latitude", airport.latitude},
            {"longitude", airport.longitude}
        };
        
        // TODO: Fetch actual runways from database
        // For now, return empty with instructions
        response["message"] = "Implement runway fetching to generate OLS for all runways";
        response["runway_count"] = airport.runway_count;
        
        /*
        // Example of what the full implementation would look like:
        auto runways = airportRepository_.fetchRunwaysByAirport(airport_icao);
        
        for (const auto& runway : runways) {
            auto surfaces = generator_.generateSurfacesForRunway(
                runway, 
                airport.elevation_ft,
                default_category,
                default_category
            );
            
            for (const auto& surface_set : surfaces) {
                auto geojson = surface_set.toGeoJSON();
                for (const auto& feature : geojson["features"]) {
                    response["features"].push_back(feature);
                }
            }
        }
        */
        
        return successResponse(response);
        
    } catch (const std::exception& e) {
        logger_->error("Error generating OLS for airport: {}", e.what());
        return errorResponse(500, "Internal server error");
    }
}

crow::response OLSSurfaceController::checkPenetration(const crow::request& req) {
    try {
        logger_->debug("Checking OLS penetration");
        
        auto body = nlohmann::json::parse(req.body);
        
        // Validate required fields
        if (!body.contains("obstacles") || !body["obstacles"].is_array()) {
            return errorResponse(400, "Missing 'obstacles' array in request body");
        }
        
        if (!body.contains("airport_icao") || !body.contains("runway_ident")) {
            return errorResponse(400, "Missing 'airport_icao' or 'runway_ident'");
        }
        
        std::string airport_icao = body["airport_icao"];
        std::string runway_ident = body["runway_ident"];
        
        // Get airport
        Airport airport;
        try {
            airport = airportRepository_.fetchAirportByIcao(airport_icao);
        } catch (const std::exception& e) {
            return errorResponse(404, "Airport not found");
        }
        
        // Generate OLS for the runway
        ApproachCategory approach_category = ApproachCategory::NonPrecisionApproach;
        if (body.contains("approach_category")) {
            approach_category = parseApproachCategory(body["approach_category"]);
        }
        
        RunwayEndData runway_end;
        runway_end.designator = runway_ident;
        
        // Parse runway heading
        int runway_number = 0;
        std::string numeric_part;
        for (char c : runway_ident) {
            if (std::isdigit(c)) numeric_part += c;
        }
        runway_number = std::stoi(numeric_part);
        
        runway_end.heading_deg = runway_number * 10.0;
        runway_end.threshold_lat = airport.latitude;
        runway_end.threshold_lon = airport.longitude;
        runway_end.threshold_elevation_m = airport.elevation_ft * 0.3048;
        runway_end.runway_length_m = airport.longest_runway_ft * 0.3048;
        runway_end.runway_width_m = 45;
        runway_end.code_number = OLSSurfaceGenerator::determineCodeNumber(runway_end.runway_length_m);
        runway_end.code_letter = OLSSurfaceGenerator::determineCodeLetter(runway_end.runway_width_m);
        runway_end.approach_category = approach_category;
        
        RunwayOLSSurfaces surfaces = generator_.generateSurfaces(runway_end);
        
        // Check each obstacle
        nlohmann::json results = nlohmann::json::array();
        
        for (const auto& obstacle : body["obstacles"]) {
            if (!obstacle.contains("latitude") || !obstacle.contains("longitude") || 
                !obstacle.contains("elevation_m")) {
                continue;
            }
            
            GeoPoint3D point(
                obstacle["latitude"].get<double>(),
                obstacle["longitude"].get<double>(),
                obstacle["elevation_m"].get<double>()
            );
            
            double penetration = generator_.checkPenetration(point, surfaces);
            
            nlohmann::json result;
            result["latitude"] = point.latitude;
            result["longitude"] = point.longitude;
            result["elevation_m"] = point.elevation;
            result["penetration_m"] = penetration;
            result["penetrates"] = (penetration > 0);
            
            if (obstacle.contains("id")) {
                result["id"] = obstacle["id"];
            }
            if (obstacle.contains("name")) {
                result["name"] = obstacle["name"];
            }
            
            results.push_back(result);
        }
        
        nlohmann::json response;
        response["airport_icao"] = airport_icao;
        response["runway_ident"] = runway_ident;
        response["results"] = results;
        response["obstacles_checked"] = results.size();
        response["penetrations_found"] = std::count_if(
            results.begin(), results.end(),
            [](const nlohmann::json& r) { return r["penetrates"].get<bool>(); }
        );
        
        return successResponse(response);
        
    } catch (const nlohmann::json::exception& e) {
        logger_->error("Invalid JSON in penetration check request: {}", e.what());
        return errorResponse(400, "Invalid JSON format");
    } catch (const std::exception& e) {
        logger_->error("Error checking penetration: {}", e.what());
        return errorResponse(500, "Internal server error");
    }
}

crow::response OLSSurfaceController::getOLSParameters(const crow::request& req) {
    try {
        // Parse query parameters
        int code_number_int = 4;  // Default
        const char* code_param = req.url_params.get("code_number");
        if (code_param) {
            code_number_int = std::stoi(code_param);
            if (code_number_int < 1 || code_number_int > 4) {
                return errorResponse(400, "code_number must be between 1 and 4");
            }
        }
        
        std::string code_letter_str = "C";  // Default
        const char* letter_param = req.url_params.get("code_letter");
        if (letter_param) {
            code_letter_str = letter_param;
        }
        
        ApproachCategory approach_category = ApproachCategory::NonPrecisionApproach;
        const char* cat_param = req.url_params.get("approach_category");
        if (cat_param) {
            approach_category = parseApproachCategory(cat_param);
        }
        
        RunwayCodeNumber code_number = static_cast<RunwayCodeNumber>(code_number_int);
        RunwayCodeLetter code_letter;
        
        if (code_letter_str == "A") code_letter = RunwayCodeLetter::A;
        else if (code_letter_str == "B") code_letter = RunwayCodeLetter::B;
        else if (code_letter_str == "C") code_letter = RunwayCodeLetter::C;
        else if (code_letter_str == "D") code_letter = RunwayCodeLetter::D;
        else if (code_letter_str == "E") code_letter = RunwayCodeLetter::E;
        else if (code_letter_str == "F") code_letter = RunwayCodeLetter::F;
        else return errorResponse(400, "code_letter must be A, B, C, D, E, or F");
        
        OLSParameters params = generator_.getOLSParameters(
            code_number, code_letter, approach_category);
        
        // Convert to JSON
        nlohmann::json response;
        response["code_number"] = static_cast<int>(params.code_number);
        response["code_letter"] = code_letter_str;
        response["approach_category"] = static_cast<int>(params.approach_category);
        
        response["approach_surface"] = {
            {"length_inner_edge_m", params.approach.length_inner_edge},
            {"distance_from_threshold_m", params.approach.distance_from_threshold},
            {"divergence_percent", params.approach.divergence},
            {"first_section_length_m", params.approach.first_section_length},
            {"first_section_slope_percent", params.approach.first_section_slope},
            {"second_section_length_m", params.approach.second_section_length},
            {"second_section_slope_percent", params.approach.second_section_slope},
            {"horizontal_section_length_m", params.approach.horizontal_section_length},
            {"total_length_m", params.approach.total_length}
        };
        
        response["takeoff_climb_surface"] = {
            {"length_inner_edge_m", params.takeoff_climb.length_inner_edge},
            {"distance_from_end_m", params.takeoff_climb.distance_from_end},
            {"divergence_percent", params.takeoff_climb.divergence},
            {"final_width_m", params.takeoff_climb.final_width},
            {"length_m", params.takeoff_climb.length},
            {"slope_percent", params.takeoff_climb.slope}
        };
        
        response["transitional_surface"] = {
            {"slope_percent", params.transitional.slope},
            {"height_limit_m", params.transitional.height_limit}
        };
        
        response["inner_horizontal_surface"] = {
            {"radius_m", params.inner_horizontal.radius},
            {"height_m", params.inner_horizontal.height}
        };
        
        response["conical_surface"] = {
            {"slope_percent", params.conical.slope},
            {"height_m", params.conical.height}
        };
        
        if (params.outer_horizontal) {
            response["outer_horizontal_surface"] = {
                {"radius_m", params.outer_horizontal->radius},
                {"height_m", params.outer_horizontal->height}
            };
        }
        
        if (params.inner_approach) {
            response["inner_approach_surface"] = {
                {"width_m", params.inner_approach->width},
                {"distance_from_threshold_m", params.inner_approach->distance_from_threshold},
                {"length_m", params.inner_approach->length},
                {"slope_percent", params.inner_approach->slope}
            };
        }
        
        if (params.inner_transitional) {
            response["inner_transitional_surface"] = {
                {"slope_percent", params.inner_transitional->slope}
            };
        }
        
        if (params.balked_landing) {
            response["balked_landing_surface"] = {
                {"length_inner_edge_m", params.balked_landing->length_inner_edge},
                {"distance_from_threshold_m", params.balked_landing->distance_from_threshold},
                {"divergence_percent", params.balked_landing->divergence},
                {"slope_percent", params.balked_landing->slope}
            };
        }
        
        return successResponse(response);
        
    } catch (const std::exception& e) {
        logger_->error("Error getting OLS parameters: {}", e.what());
        return errorResponse(500, "Internal server error");
    }
}

ApproachCategory OLSSurfaceController::parseApproachCategory(const std::string& category_str) {
    std::string lower = category_str;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    
    if (lower == "non_instrument" || lower == "noninstrument" || lower == "0") {
        return ApproachCategory::NonInstrument;
    }
    if (lower == "non_precision" || lower == "nonprecision" || lower == "npa" || lower == "1") {
        return ApproachCategory::NonPrecisionApproach;
    }
    if (lower == "cat1" || lower == "cat_1" || lower == "precision_cat1" || lower == "2") {
        return ApproachCategory::PrecisionCategoryI;
    }
    if (lower == "cat2" || lower == "cat_2" || lower == "precision_cat2" || lower == "3") {
        return ApproachCategory::PrecisionCategoryII;
    }
    if (lower == "cat3" || lower == "cat_3" || lower == "precision_cat3" || lower == "4") {
        return ApproachCategory::PrecisionCategoryIII;
    }
    
    // Default
    return ApproachCategory::NonPrecisionApproach;
}

crow::response OLSSurfaceController::errorResponse(int code, const std::string& message) {
    nlohmann::json response;
    response["error"] = true;
    response["code"] = code;
    response["message"] = message;
    
    crow::response res(code, response.dump());
    res.add_header("Content-Type", "application/json");
    res.add_header("Access-Control-Allow-Origin", "*");
    return res;
}

crow::response OLSSurfaceController::successResponse(const nlohmann::json& data) {
    crow::response res(200, data.dump());
    res.add_header("Content-Type", "application/json");
    res.add_header("Access-Control-Allow-Origin", "*");
    return res;
}

} // namespace aeronautical
