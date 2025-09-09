#include "Waypoint.h"
#include <json.hpp>

namespace aeronautical {

nlohmann::json Waypoint::toJson() const {
    try {
        nlohmann::json json_obj = nlohmann::json::object();
        
        // Basic information - ensure all values are properly handled
        json_obj["id"] = id;
        json_obj["waypoint_code"] = waypoint_code.empty() ? "" : waypoint_code;
        json_obj["name"] = name.empty() ? "" : name;
        
        // Coordinates - ensure they're valid numbers
        json_obj["latitude"] = std::isfinite(latitude) ? latitude : 0.0;
        json_obj["longitude"] = std::isfinite(longitude) ? longitude : 0.0;
        json_obj["elevation_ft"] = elevation_ft;
        
        // String fields
        json_obj["waypoint_type"] = waypoint_type.empty() ? "" : waypoint_type;
        json_obj["country_code"] = country_code.empty() ? "" : country_code;
        json_obj["country_name"] = country_name.empty() ? "" : country_name;
        json_obj["region"] = region.empty() ? "" : region;
        json_obj["frequency"] = frequency.empty() ? "" : frequency;
        json_obj["usage_type"] = usage_type.empty() ? "" : usage_type;
        
        // Boolean field
        json_obj["is_active"] = is_active;
        
        return json_obj;
        
    } catch (const std::exception& e) {
        // If JSON creation fails, return a minimal valid object
        return nlohmann::json{
            {"id", id},
            {"waypoint_code", waypoint_code},
            {"name", name},
            {"error", "JSON serialization error"}
        };
    }
}

} // namespace aeronautical