#pragma once

#include <string>
#include <json.hpp>

namespace aeronautical {

struct Airport {
    int id;
    std::string icao_code;
    std::string iata_code;
    std::string name;
    std::string full_name;
    double latitude;
    double longitude;
    int elevation_ft;
    std::string airport_type;
    std::string municipality;
    std::string region;
    std::string country_code;
    std::string country_name;
    bool is_active;
    bool has_tower;
    bool has_ils;
    int runway_count;
    int longest_runway_ft;
    
    nlohmann::json toJson() const;
};

struct AirportRunway {
    int id;
    int airport_id;
    std::string runway_identifier;
    int length_ft;
    int width_ft;
    std::string surface_type;
    std::string le_ident;
    double le_heading_deg;
    double le_latitude;
    double le_longitude;
    std::string he_ident;
    double he_heading_deg;
    double he_latitude;
    double he_longitude;
    bool is_active;
    
    nlohmann::json toJson() const;
};

} // namespace aeronautical