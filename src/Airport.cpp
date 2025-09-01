#include "Airport.h"
#include <json.hpp>

namespace aeronautical {
// Airport and AirportRunway toJson() methods remain the same
nlohmann::json Airport::toJson() const {
    return nlohmann::json{
        {"id", id}, {"icao_code", icao_code}, {"iata_code", iata_code},
        {"name", name}, {"full_name", full_name}, {"latitude", latitude},
        {"longitude", longitude}, {"elevation_ft", elevation_ft},
        {"airport_type", airport_type}, {"municipality", municipality},
        {"region", region}, {"country_code", country_code},
        {"country_name", country_name}, {"is_active", is_active},
        {"has_tower", has_tower}, {"has_ils", has_ils},
        {"runway_count", runway_count}, {"longest_runway_ft", longest_runway_ft}
    };
}

nlohmann::json AirportRunway::toJson() const { /* ... */ return nlohmann::json{}; }
}