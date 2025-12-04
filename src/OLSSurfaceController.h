#pragma once

#include <crow.h>
#include <json.hpp>
#include <memory>
#include <spdlog/spdlog.h>
#include "OLSSurfaceGenerator.h"
#include "AirportRepository.h"

namespace aeronautical {

/**
 * REST API Controller for ICAO Annex 14 Obstacle Limitation Surfaces
 * 
 * Endpoints:
 *   GET  /api/ols/runway/{airport_icao}/{runway_id}  - Generate OLS for specific runway
 *   GET  /api/ols/airport/{airport_icao}             - Generate OLS for all runways at airport
 *   POST /api/ols/check-penetration                  - Check if obstacles penetrate OLS
 *   GET  /api/ols/parameters                         - Get OLS parameters for classification
 */
class OLSSurfaceController {
public:
    OLSSurfaceController();
    ~OLSSurfaceController() = default;
    
    void registerRoutes(crow::SimpleApp& app);
    
private:
    std::shared_ptr<spdlog::logger> logger_;
    OLSSurfaceGenerator generator_;
    AirportRepository airportRepository_;
    
    // Route handlers
    crow::response getOLSForRunway(const crow::request& req, 
                                   const std::string& airport_icao,
                                   const std::string& runway_ident);
    
    crow::response getOLSForAirport(const crow::request& req,
                                    const std::string& airport_icao);
    
    crow::response checkPenetration(const crow::request& req);
    
    crow::response getOLSParameters(const crow::request& req);
    
    // Helper methods
    ApproachCategory parseApproachCategory(const std::string& category_str);
    crow::response errorResponse(int code, const std::string& message);
    crow::response successResponse(const nlohmann::json& data);
};

} // namespace aeronautical
