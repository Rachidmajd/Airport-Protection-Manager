
#pragma once

#include <crow.h>
#include <json.hpp>
#include <string>
#include <vector>
#include <memory>
#include <spdlog/spdlog.h>
#include "AirportRepository.h" // Include the new repository

namespace aeronautical {

// Airport and AirportRunway struct definitions remain the same
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

class AirportController {
public:
    AirportController();
    ~AirportController() = default;
    
    void registerRoutes(crow::SimpleApp& app);
    
private:
    std::shared_ptr<spdlog::logger> logger_;
    AirportRepository airportRepository_; // Add repository member

    // API handlers (these stay the same)
    crow::response getAllAirports(const crow::request& req);
    crow::response getAirportByIcao(const std::string& icao_code);
    crow::response getAirportsByCountry(const std::string& country_code);
    crow::response getAirportsInBounds(const crow::request& req);
    crow::response getAirportRunways(const std::string& icao_code);
    crow::response searchAirports(const crow::request& req);
        
    // Helper methods (these stay the same)
    bool validateBounds(double min_lat, double max_lat, double min_lng, double max_lng);
    nlohmann::json createErrorResponse(const std::string& message, int code = 400);
    nlohmann::json createSuccessResponse(const nlohmann::json& data);
};

} // namespace aeronautical