#pragma once

#include <crow.h>
#include <json.hpp>
#include <string>
#include <vector>
#include <memory>
#include <spdlog/spdlog.h>
#include "Waypoint.h"
#include "WaypointRepository.h"

namespace aeronautical {

class WaypointController {
public:
    WaypointController();
    ~WaypointController() = default;
    
    void registerRoutes(crow::SimpleApp& app);
    
private:
    std::shared_ptr<spdlog::logger> logger_;
    WaypointRepository waypointRepository_;

    // API handlers - read-only operations
    crow::response getAllWaypoints(const crow::request& req);
    crow::response getWaypointByCode(const std::string& waypoint_code);
    crow::response getWaypointsByCountry(const std::string& country_code);
    crow::response getWaypointsInBounds(const crow::request& req);
    crow::response searchWaypoints(const crow::request& req);
    crow::response getWaypointsByType(const std::string& waypoint_type);
    crow::response getWaypointsByUsage(const std::string& usage_type);
        
    // Helper methods
    bool validateBounds(double min_lat, double max_lat, double min_lng, double max_lng);
    nlohmann::json createErrorResponse(const std::string& message, int code = 400);
    nlohmann::json createSuccessResponse(const nlohmann::json& data);
};

} // namespace aeronautical