#pragma once

#include "Waypoint.h"

#include <vector>
#include <string>
#include <optional>
#include <spdlog/spdlog.h>

#ifdef USE_MYSQL_C_API
    #include <mysql/mysql.h>
#else
    #include <mysqlx/xdevapi.h>
#endif

namespace aeronautical {

class WaypointRepository {
public:
    WaypointRepository();
    
    // Read-only operations
    std::vector<Waypoint> fetchAllWaypoints(const std::string& filter_type = "", bool active_only = true);
    std::optional<Waypoint> fetchWaypointByCode(const std::string& waypoint_code);
    std::vector<Waypoint> fetchWaypointsByCountry(const std::string& country_code, bool active_only = true);
    std::vector<Waypoint> fetchWaypointsInBounds(double min_lat, double max_lat, 
                                                 double min_lng, double max_lng, 
                                                 const std::string& filter_type = "");
    std::vector<Waypoint> searchWaypointsByQuery(const std::string& query, int limit = 20);
    std::vector<Waypoint> fetchWaypointsByType(const std::string& waypoint_type, bool active_only = true);
    std::vector<Waypoint> fetchWaypointsByUsage(const std::string& usage_type, bool active_only = true);

private:
    std::shared_ptr<spdlog::logger> logger_;
    
#ifdef USE_MYSQL_C_API
    Waypoint rowToWaypoint(MYSQL_ROW row, unsigned long* lengths);
#else
    Waypoint rowToWaypoint(mysqlx::Row& row);
#endif
};

} // namespace aeronautical