#pragma once

#include <crow.h>
#include <json.hpp>
#include <string>
#include <vector>
#include <memory>
#include <spdlog/spdlog.h>
#include "Airport.h"         // Include the Airport definitions
#include "AirportRepository.h" // Include the repository

namespace aeronautical {

class AirportController {
public:
    AirportController();
    ~AirportController() = default;
    
    void registerRoutes(crow::SimpleApp& app);
    
private:
    std::shared_ptr<spdlog::logger> logger_;
    AirportRepository airportRepository_; // Add repository member

    // API handlers
    crow::response getAllAirports(const crow::request& req);
    crow::response getAirportByIcao(const std::string& icao_code);
    crow::response getAirportsByCountry(const std::string& country_code);
    crow::response getAirportsInBounds(const crow::request& req);
    crow::response getAirportRunways(const std::string& icao_code);
    crow::response searchAirports(const crow::request& req);
        
    // Helper methods
    bool validateBounds(double min_lat, double max_lat, double min_lng, double max_lng);
    nlohmann::json createErrorResponse(const std::string& message, int code = 400);
    nlohmann::json createSuccessResponse(const nlohmann::json& data);
};

} // namespace aeronautical