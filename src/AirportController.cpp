#include "AirportController.h"
#include <string>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace aeronautical {

AirportController::AirportController() {
    logger_ = spdlog::stdout_color_mt("AirportController");
    logger_->set_level(spdlog::level::debug);
}

void AirportController::registerRoutes(crow::SimpleApp& app) {
    CROW_ROUTE(app, "/api/airports")([this](const crow::request& req) { 
        return getAllAirports(req); 
    });
    
    CROW_ROUTE(app, "/api/airports/icao/<string>")([this](const std::string& icao) { 
        return getAirportByIcao(icao); 
    });
    
    CROW_ROUTE(app, "/api/airports/country/<string>")([this](const std::string& country) { 
        return getAirportsByCountry(country); 
    });
    
    CROW_ROUTE(app, "/api/airports/bounds")([this](const crow::request& req) { 
        return getAirportsInBounds(req); 
    });
    
    CROW_ROUTE(app, "/api/airports/search")([this](const crow::request& req) { 
        return searchAirports(req); 
    });
    
    CROW_ROUTE(app, "/api/airports/runways/<string>")([this](const std::string& icao) { 
        return getAirportRunways(icao); 
    });
}

crow::response AirportController::getAllAirports(const crow::request& req) {
    try {
        logger_->debug("Getting all airports");
        
        std::string filter_type = req.url_params.get("type") ? req.url_params.get("type") : "";
        bool active_only = !req.url_params.get("active_only") || std::string(req.url_params.get("active_only")) != "false";
        
        auto airports = airportRepository_.fetchAllAirports(filter_type, active_only);
        nlohmann::json json_airports = nlohmann::json::array();
        
        for (const auto& airport : airports) {
            try {
                json_airports.push_back(airport.toJson());
            } catch (const std::exception& e) {
                logger_->warn("Error serializing airport {}: {}", airport.icao_code, e.what());
                continue; // Skip this airport and continue with others
            }
        }
        
        logger_->debug("Successfully serialized {} airports", json_airports.size());
        return crow::response(200, createSuccessResponse(json_airports).dump());
        
    } catch (const std::exception& e) {
        logger_->error("Error in getAllAirports: {}", e.what());
        return crow::response(500, createErrorResponse("Internal server error", 500).dump());
    }
}

crow::response AirportController::getAirportByIcao(const std::string& icao_code) {
    try {
        logger_->debug("Getting airport by ICAO: {}", icao_code);
        
        auto airport = airportRepository_.fetchAirportByIcao(icao_code);
        return crow::response(200, createSuccessResponse(airport.toJson()).dump());
        
    } catch (const std::runtime_error& e) {
        logger_->warn("Could not find airport with ICAO '{}': {}", icao_code, e.what());
        return crow::response(404, createErrorResponse("Airport not found").dump());
    } catch (const std::exception& e) {
        logger_->error("Error in getAirportByIcao for '{}': {}", icao_code, e.what());
        return crow::response(500, createErrorResponse("Internal server error", 500).dump());
    }
}

crow::response AirportController::getAirportsByCountry(const std::string& country_code) {
    try {
        logger_->debug("Getting airports by country: {}", country_code);
        
        auto airports = airportRepository_.fetchAirportsByCountry(country_code, true);
        nlohmann::json json_airports = nlohmann::json::array();
        
        for (const auto& airport : airports) {
            try {
                json_airports.push_back(airport.toJson());
            } catch (const std::exception& e) {
                logger_->warn("Error serializing airport {} for country {}: {}", airport.icao_code, country_code, e.what());
                continue;
            }
        }
        
        logger_->debug("Successfully found {} airports for country {}", json_airports.size(), country_code);
        return crow::response(200, createSuccessResponse(json_airports).dump());
        
    } catch (const std::exception& e) {
        logger_->error("Error in getAirportsByCountry for '{}': {}", country_code, e.what());
        return crow::response(500, createErrorResponse("Internal server error", 500).dump());
    }
}

crow::response AirportController::getAirportsInBounds(const crow::request& req) {
    try {
        logger_->debug("Getting airports in bounds with params: {}", req.url);
        
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
        auto airports = airportRepository_.fetchAirportsInBounds(min_lat, max_lat, min_lng, max_lng, filter_type);

        nlohmann::json json_airports = nlohmann::json::array();
        for (const auto& airport : airports) {
            try {
                json_airports.push_back(airport.toJson());
            } catch (const std::exception& e) {
                logger_->warn("Error serializing airport {} in bounds: {}", airport.icao_code, e.what());
                continue;
            }
        }
        
        logger_->debug("Successfully found {} airports in bounds", json_airports.size());
        return crow::response(200, createSuccessResponse(json_airports).dump());
        
    } catch (const std::exception& e) {
        logger_->error("Unexpected error in getAirportsInBounds: {}", e.what());
        return crow::response(500, createErrorResponse("Internal server error", 500).dump());
    }
}

crow::response AirportController::searchAirports(const crow::request& req) {
    try {
        logger_->debug("Searching airports with params: {}", req.url);
        
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
        
        auto airports = airportRepository_.searchAirportsByQuery(query, limit);
        nlohmann::json json_airports = nlohmann::json::array();
        
        for (const auto& airport : airports) {
            try {
                json_airports.push_back(airport.toJson());
            } catch (const std::exception& e) {
                logger_->warn("Error serializing airport {} in search: {}", airport.icao_code, e.what());
                continue;
            }
        }
        
        logger_->debug("Search for '{}' returned {} airports", query, json_airports.size());
        return crow::response(200, createSuccessResponse(json_airports).dump());
        
    } catch (const std::exception& e) {
        logger_->error("Error in searchAirports: {}", e.what());
        return crow::response(500, createErrorResponse("Internal server error", 500).dump());
    }
}

crow::response AirportController::getAirportRunways(const std::string& icao_code) {
    try {
        logger_->debug("Getting runways for airport: {}", icao_code);
        
        // For now, return a not implemented response with proper JSON structure
        nlohmann::json empty_runways = nlohmann::json::array();
        
        logger_->debug("Runway endpoint not yet implemented for {}", icao_code);
        return crow::response(200, createSuccessResponse(empty_runways).dump());
        
    } catch (const std::exception& e) {
        logger_->error("Error in getAirportRunways for '{}': {}", icao_code, e.what());
        return crow::response(500, createErrorResponse("Internal server error", 500).dump());
    }
}

// Helper Methods
bool AirportController::validateBounds(double min_lat, double max_lat, double min_lng, double max_lng) {
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

nlohmann::json AirportController::createErrorResponse(const std::string& message, int code) {
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

nlohmann::json AirportController::createSuccessResponse(const nlohmann::json& data) {
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