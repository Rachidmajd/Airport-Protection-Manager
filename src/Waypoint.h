#pragma once

#include <string>
#include <json.hpp>

namespace aeronautical {

struct Waypoint {
    int id;
    std::string waypoint_code;
    std::string name;
    double latitude;
    double longitude;
    int elevation_ft;
    std::string waypoint_type;  // "VOR", "NDB", "GPS", "TACAN", "DME", "FIX", etc.
    std::string country_code;
    std::string country_name;
    std::string region;
    std::string frequency;  // For radio navaids (empty for GPS waypoints)
    std::string usage_type; // "ENROUTE", "TERMINAL", "APPROACH", "SID", "STAR"
    bool is_active;
    
    nlohmann::json toJson() const;
};

} // namespace aeronautical