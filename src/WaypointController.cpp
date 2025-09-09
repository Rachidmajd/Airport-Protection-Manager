#include "WaypointController.h"
#include <string>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace aeronautical {

WaypointController::WaypointController() {
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
    logger_->set_level(spdlog::level::debug);
}

void WaypointController::registerRoutes(crow::SimpleApp& app) {
    // GET /api/waypoints - Get all waypoints with optional filtering
    CROW_ROUTE(app, "/api/waypoints")([this](const crow::request& req) { 
        return getAllWaypoints(req); 
    });
    
    // GET /api/waypoints/code/:code - Get specific waypoint by code
    CROW_ROUTE(app, "/api/waypoints/code/<string>")([this](const std::string& code) { 
        return getWaypointByCode(code); 
    });
    
    // GET /api/waypoints/country/:country - Get waypoints by country
    CROW_ROUTE(app, "/api/waypoints/country/<string>")([this](const std::string& country) { 
        return getWaypointsByCountry(country); 
    });
    
    // GET /api/waypoints/type/:type - Get waypoints by type (VOR, NDB, GPS, etc.)
    CROW_ROUTE(app, "/api/waypoints/type/<string>")([this](const std::string& type) { 
        return getWaypointsByType(type); 
    });
    
    // GET /api/waypoints/usage/:usage - Get waypoints by usage (ENROUTE, TERMINAL, etc.)
    CROW_ROUTE(app, "/api/waypoints/usage/<string>")([this](const std::string& usage) { 
        return getWaypointsByUsage(usage); 
    });
    
    // GET /api/waypoints/bounds - Get waypoints within geographical bounds
    CROW_ROUTE(app, "/api/waypoints/bounds")([this](const crow::request& req) { 
        return getWaypointsInBounds(req); 
    });
    
    // GET /api/waypoints/search - Search waypoints
    CROW_ROUTE(app, "/api/waypoints/search")([this](const crow::request& req) { 
        return searchWaypoints(req); 
    });
    
    logger_->info("Waypoint routes registered");
}

crow::response WaypointController::getAllWaypoints(const crow::request& req) {
    try {
        logger_->debug("Getting all waypoints");
        
        std::string filter_type = req.url_params.get("type") ? req.url_params.get("type") : "";
        bool active_only = !req.url_params.get("active_only") || std::string(req.url_params.get("active_only")) != "false";
        
        auto waypoints = waypointRepository_.fetchAllWaypoints(filter_type, active_only);
        nlohmann::json json_waypoints = nlohmann::json::array();
        
        for (const auto& waypoint : waypoints) {
            try {
                json_waypoints.push_back(waypoint.toJson());
            } catch (const std::exception& e) {
                logger_->warn("Error serializing waypoint {}: {}", waypoint.waypoint_code, e.what());
                continue; // Skip this waypoint and continue with others
            }
        }
        
        logger_->debug("Successfully serialized {} waypoints", json_waypoints.size());
        return crow::response(200, createSuccessResponse(json_waypoints).dump());
        
    } catch (const std::exception& e) {
        logger_->error("Error in getAllWaypoints: {}", e.what());
        return crow::response(500, createErrorResponse("Internal server error", 500).dump());
    }
}

crow::response WaypointController::getWaypointByCode(const std::string& waypoint_code) {
    try {
        logger_->debug("Getting waypoint by code: {}", waypoint_code);
        
        auto waypoint = waypointRepository_.fetchWaypointByCode(waypoint_code);
        
        if (!waypoint) {
            return crow::response(404, createErrorResponse("Waypoint not found").dump());
        }
        
        return crow::response(200, createSuccessResponse(waypoint->toJson()).dump());
        
    } catch (const std::exception& e) {
        logger_->error("Error in getWaypointByCode for '{}': {}", waypoint_code, e.what());
        return crow::response(500, createErrorResponse("Internal server error", 500).dump());
    }
}

crow::response WaypointController::getWaypointsByCountry(const std::string& country_code) {
    try {
        logger_->debug("Getting waypoints by country: {}", country_code);
        
        auto waypoints = waypointRepository_.fetchWaypointsByCountry(country_code, true);
        nlohmann::json json_waypoints = nlohmann::json::array();
        
        for (const auto& waypoint : waypoints) {
            try {
                json_waypoints.push_back(waypoint.toJson());
            } catch (const std::exception& e) {
                logger_->warn("Error serializing waypoint {} for country {}: {}", waypoint.waypoint_code, country_code, e.what());
                continue;
            }
        }
        
        logger_->debug("Successfully found {} waypoints for country {}", json_waypoints.size(), country_code);
        return crow::response(200, createSuccessResponse(json_waypoints).dump());
        
    } catch (const std::exception& e) {
        logger_->error("Error in getWaypointsByCountry for '{}': {}", country_code, e.what());
        return crow::response(500, createErrorResponse("Internal server error", 500).dump());
    }
}

crow::response WaypointController::getWaypointsByType(const std::string& waypoint_type) {
    try {
        logger_->debug("Getting waypoints by type: {}", waypoint_type);
        
        auto waypoints = waypointRepository_.fetchWaypointsByType(waypoint_type, true);
        nlohmann::json json_waypoints = nlohmann::json::array();
        
        for (const auto& waypoint : waypoints) {
            try {
                json_waypoints.push_back(waypoint.toJson());
            } catch (const std::exception& e) {
                logger_->warn("Error serializing waypoint {} for type {}: {}", waypoint.waypoint_code, waypoint_type, e.what());
                continue;
            }
        }
        
        logger_->debug("Successfully found {} waypoints of type {}", json_waypoints.size(), waypoint_type);
        return crow::response(200, createSuccessResponse(json_waypoints).dump());
        
    } catch (const std::exception& e) {
        logger_->error("Error in getWaypointsByType for '{}': {}", waypoint_type, e.what());
        return crow::response(500, createErrorResponse("Internal server error", 500).dump());
    }
}

crow::response WaypointController::getWaypointsByUsage(const std::string& usage_type) {
    try {
        logger_->debug("Getting waypoints by usage: {}", usage_type);
        
        auto waypoints = waypointRepository_.fetchWaypointsByUsage(usage_type, true);
        nlohmann::json json_waypoints = nlohmann::json::array();
        
        for (const auto& waypoint : waypoints) {
            try {
                json_waypoints.push_back(waypoint.toJson());
            } catch (const std::exception& e) {
                logger_->warn("Error serializing waypoint {} for usage {}: {}", waypoint.waypoint_code, usage_type, e.what());
                continue;
            }
        }
        
        logger_->debug("Successfully found {} waypoints for usage {}", json_waypoints.size(), usage_type);
        return crow::response(200, createSuccessResponse(json_waypoints).dump());
        
    } catch (const std::exception& e) {
        logger_->error("Error in getWaypointsByUsage for '{}': {}", usage_type, e.what());
        return crow::response(500, createErrorResponse("Internal server error", 500).dump());
    }
}

crow::response WaypointController::getWaypointsInBounds(const crow::request& req) {
    try {
        logger_->debug("Getting waypoints in bounds with params: {}", req.url);
        
        // Validate required parameters exist
        const char* min_lat_param = req.url_params.get("min_lat");
        const char* max_lat_param = req.url_params.get("max_lat");
        const char* min_lng_param = req.url_params.get("min_lng");
        const char* max_lng_param = req.url_params.get("max_lng");
        
        if (!min_lat_param || !max_lat_param || !min_lng_param || !max_lng_param) {
            logger_->warn("Missing required boundary parameters");
            return crow::response(400, createErrorResponse("Missing required parameters: min_lat, max_lat, min_lng, max_lng").dump());
        }
        
        // Parse parameters with error checking
        double min_lat, max_lat, min_lng, max_lng;
        try {
            min_lat = std::stod(min_lat_param);
            max_lat = std::stod(max_lat_param);
            min_lng = std::stod(min_lng_param);
            max_lng = std::stod(max_lng_param);
        } catch (const std::invalid_argument& e) {
            logger_->warn("Invalid boundary parameter format: {}", e.what());
            return crow::response(400, createErrorResponse("Invalid number format in boundary parameters").dump());
        } catch (const std::out_of_range& e) {
            logger_->warn("Boundary parameter out of range: {}", e.what());
            return crow::response(400, createErrorResponse("Boundary parameter out of range").dump());
        }
        
        // Validate bounds
        if (!validateBounds(min_lat, max_lat, min_lng, max_lng)) {
            logger_->warn("Invalid boundary values: lat({}, {}), lng({}, {})", min_lat, max_lat, min_lng, max_lng);
            return crow::response(400, createErrorResponse("Invalid boundary values").dump());
        }
        
        logger_->debug("Validated bounds: lat({}, {}), lng({}, {})", min_lat, max_lat, min_lng, max_lng);
        
        std::string filter_type = req.url_params.get("type") ? req.url_params.get("type") : "";
        auto waypoints = waypointRepository_.fetchWaypointsInBounds(min_lat, max_lat, min_lng, max_lng, filter_type);

        nlohmann::json json_waypoints = nlohmann::json::array();
        for (const auto& waypoint : waypoints) {
            try {
                json_waypoints.push_back(waypoint.toJson());
            } catch (const std::exception& e) {
                logger_->warn("Error serializing waypoint {} in bounds: {}", waypoint.waypoint_code, e.what());
                continue;
            }
        }
        
        logger_->debug("Successfully found {} waypoints in bounds", json_waypoints.size());
        return crow::response(200, createSuccessResponse(json_waypoints).dump());
        
    } catch (const std::exception& e) {
        logger_->error("Unexpected error in getWaypointsInBounds: {}", e.what());
        return crow::response(500, createErrorResponse("Internal server error", 500).dump());
    }
}

crow::response WaypointController::searchWaypoints(const crow::request& req) {
    try {
        logger_->debug("Searching waypoints with params: {}", req.url);
        
        auto query = req.url_params.get("q");
        if (!query) {
            return crow::response(400, createErrorResponse("Missing search query 'q'").dump());
        }
        
        int limit = 20; // Default limit
        const char* limit_param = req.url_params.get("limit");
        if (limit_param) {
            try {
                limit = std::stoi(limit_param);
                if (limit <= 0 || limit > 100) {
                    limit = 20; // Reset to default if invalid
                }
            } catch (const std::exception& e) {
                logger_->warn("Invalid limit parameter, using default: {}", e.what());
                limit = 20;
            }
        }

        logger_->debug("Searching for '{}' with limit {}", query, limit);
        
        auto waypoints = waypointRepository_.searchWaypointsByQuery(query, limit);
        nlohmann::json json_waypoints = nlohmann::json::array();
        
        for (const auto& waypoint : waypoints) {
            try {
                json_waypoints.push_back(waypoint.toJson());
            } catch (const std::exception& e) {
                logger_->warn("Error serializing waypoint {} in search: {}", waypoint.waypoint_code, e.what());
                continue;
            }
        }
        
        logger_->debug("Search for '{}' returned {} waypoints", query, json_waypoints.size());
        return crow::response(200, createSuccessResponse(json_waypoints).dump());
        
    } catch (const std::exception& e) {
        logger_->error("Error in searchWaypoints: {}", e.what());
        return crow::response(500, createErrorResponse("Internal server error", 500).dump());
    }
}

// Helper Methods
bool WaypointController::validateBounds(double min_lat, double max_lat, double min_lng, double max_lng) {
    // Check latitude bounds
    if (min_lat < -90.0 || min_lat > 90.0) return false;
    if (max_lat < -90.0 || max_lat > 90.0) return false;
    if (min_lat > max_lat) return false;
    
    // Check longitude bounds
    if (min_lng < -180.0 || min_lng > 180.0) return false;
    if (max_lng < -180.0 || max_lng > 180.0) return false;
    if (min_lng > max_lng) return false;
    
    return true;
}

nlohmann::json WaypointController::createErrorResponse(const std::string& message, int code) {
    try {
        return nlohmann::json{
            {"status", "error"}, 
            {"code", code}, 
            {"message", message}
        };
    } catch (const std::exception& e) {
        logger_->error("Error creating error response: {}", e.what());
        // Fallback to simple JSON object
        return nlohmann::json{
            {"status", "error"},
            {"message", message},
            {"code", code}
        };
    }
}

nlohmann::json WaypointController::createSuccessResponse(const nlohmann::json& data) {
    try {
        return nlohmann::json{
            {"status", "success"}, 
            {"data", data}
        };
    } catch (const std::exception& e) {
        logger_->error("Error creating success response: {}", e.what());
        // Fallback to simple JSON object
        return nlohmann::json{
            {"status", "error"},
            {"message", "JSON serialization error"}
        };
    }
}

} // namespace aeronautical