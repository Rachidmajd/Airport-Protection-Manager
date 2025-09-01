#pragma once

#include "Airport.h"

#include <vector>
#include <string>
#include <spdlog/spdlog.h>


namespace aeronautical {

class AirportRepository {
public:
    AirportRepository();
    std::vector<Airport> fetchAllAirports(const std::string& filter_type, bool active_only);
    Airport fetchAirportByIcao(const std::string& icao_code);
    std::vector<Airport> fetchAirportsByCountry(const std::string& country_code, bool active_only);
    std::vector<Airport> fetchAirportsInBounds(double min_lat, double max_lat, double min_lng, double max_lng, const std::string& filter_type);
    std::vector<Airport> searchAirportsByQuery(const std::string& query, int limit);

private:
    // You can add a logger here if you wish, similar to controllers
    std::shared_ptr<spdlog::logger> logger_; 
};

} // namespace aeronautical