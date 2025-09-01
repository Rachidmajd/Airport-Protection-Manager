#include "Airport.h"
#include <json.hpp>

namespace aeronautical {

nlohmann::json Airport::toJson() const {
    try {
        nlohmann::json json_obj = nlohmann::json::object();
        
        // Basic information - ensure all values are properly handled
        json_obj["id"] = id;
        json_obj["icao_code"] = icao_code.empty() ? "" : icao_code;
        json_obj["iata_code"] = iata_code.empty() ? "" : iata_code;
        json_obj["name"] = name.empty() ? "" : name;
        json_obj["full_name"] = full_name.empty() ? "" : full_name;
        
        // Coordinates - ensure they're valid numbers
        json_obj["latitude"] = std::isfinite(latitude) ? latitude : 0.0;
        json_obj["longitude"] = std::isfinite(longitude) ? longitude : 0.0;
        json_obj["elevation_ft"] = elevation_ft;
        
        // String fields
        json_obj["airport_type"] = airport_type.empty() ? "" : airport_type;
        json_obj["municipality"] = municipality.empty() ? "" : municipality;
        json_obj["region"] = region.empty() ? "" : region;
        json_obj["country_code"] = country_code.empty() ? "" : country_code;
        json_obj["country_name"] = country_name.empty() ? "" : country_name;
        
        // Boolean fields
        json_obj["is_active"] = is_active;
        json_obj["has_tower"] = has_tower;
        json_obj["has_ils"] = has_ils;
        
        // Numeric fields
        json_obj["runway_count"] = runway_count;
        json_obj["longest_runway_ft"] = longest_runway_ft;
        
        return json_obj;
        
    } catch (const std::exception& e) {
        // If JSON creation fails, return a minimal valid object
        return nlohmann::json{
            {"id", id},
            {"icao_code", icao_code},
            {"name", name},
            {"error", "JSON serialization error"}
        };
    }
}

nlohmann::json AirportRunway::toJson() const {
    try {
        nlohmann::json json_obj = nlohmann::json::object();
        
        json_obj["id"] = id;
        json_obj["airport_id"] = airport_id;
        json_obj["runway_identifier"] = runway_identifier.empty() ? "" : runway_identifier;
        json_obj["length_ft"] = length_ft;
        json_obj["width_ft"] = width_ft;
        json_obj["surface_type"] = surface_type.empty() ? "" : surface_type;
        
        // Low end
        json_obj["le_ident"] = le_ident.empty() ? "" : le_ident;
        json_obj["le_heading_deg"] = std::isfinite(le_heading_deg) ? le_heading_deg : 0.0;
        json_obj["le_latitude"] = std::isfinite(le_latitude) ? le_latitude : 0.0;
        json_obj["le_longitude"] = std::isfinite(le_longitude) ? le_longitude : 0.0;
        
        // High end
        json_obj["he_ident"] = he_ident.empty() ? "" : he_ident;
        json_obj["he_heading_deg"] = std::isfinite(he_heading_deg) ? he_heading_deg : 0.0;
        json_obj["he_latitude"] = std::isfinite(he_latitude) ? he_latitude : 0.0;
        json_obj["he_longitude"] = std::isfinite(he_longitude) ? he_longitude : 0.0;
        
        json_obj["is_active"] = is_active;
        
        return json_obj;
        
    } catch (const std::exception& e) {
        // If JSON creation fails, return a minimal valid object
        return nlohmann::json{
            {"id", id},
            {"airport_id", airport_id},
            {"runway_identifier", runway_identifier},
            {"error", "JSON serialization error"}
        };
    }
}

} // namespace aeronautical