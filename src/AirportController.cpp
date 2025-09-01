#include "AirportController.h"
#include <string>

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

AirportController::AirportController() {
    logger_ = spdlog::stdout_color_mt("AirportController");
    logger_->set_level(spdlog::level::debug);
}

void AirportController::registerRoutes(crow::SimpleApp& app) {
    CROW_ROUTE(app, "/api/airports")([this](const crow::request& req) { return getAllAirports(req); });
    CROW_ROUTE(app, "/api/airports/icao/<string>")([this](const std::string& icao) { return getAirportByIcao(icao); });
    CROW_ROUTE(app, "/api/airports/country/<string>")([this](const std::string& country) { return getAirportsByCountry(country); });
    CROW_ROUTE(app, "/api/airports/bounds")([this](const crow::request& req) { return getAirportsInBounds(req); });
    CROW_ROUTE(app, "/api/airports/search")([this](const crow::request& req) { return searchAirports(req); });
    CROW_ROUTE(app, "/api/airports/runways/<string>")([this](const std::string&) { 
        return crow::response(501, createErrorResponse("Not Implemented")); 
    });
}

crow::response AirportController::getAllAirports(const crow::request& req) {
    std::string filter_type = req.url_params.get("type") ? req.url_params.get("type") : "";
    bool active_only = !req.url_params.get("active_only") || std::string(req.url_params.get("active_only")) != "false";
    
    auto airports = airportRepository_.fetchAllAirports(filter_type, active_only);
    nlohmann::json json_airports = nlohmann::json::array();
    for (const auto& airport : airports) {
        json_airports.push_back(airport.toJson());
    }
    return crow::response(200, createSuccessResponse(json_airports));
}

crow::response AirportController::getAirportByIcao(const std::string& icao_code) {
    try {
        auto airport = airportRepository_.fetchAirportByIcao(icao_code);
        return crow::response(200, createSuccessResponse(airport.toJson()));
    } catch (const std::runtime_error& e) {
        logger_->warn("Could not find airport with ICAO '{}': {}", icao_code, e.what());
        return crow::response(404, createErrorResponse("Airport not found"));
    }
}

crow::response AirportController::getAirportsByCountry(const std::string& country_code) {
    auto airports = airportRepository_.fetchAirportsByCountry(country_code, true);
    nlohmann::json json_airports = nlohmann::json::array();
    for (const auto& airport : airports) {
        json_airports.push_back(airport.toJson());
    }
    return crow::response(200, createSuccessResponse(json_airports));
}

crow::response AirportController::getAirportsInBounds(const crow::request& req) {
    try {
        double min_lat = std::stod(req.url_params.get("min_lat"));
        double max_lat = std::stod(req.url_params.get("max_lat"));
        double min_lng = std::stod(req.url_params.get("min_lng"));
        double max_lng = std::stod(req.url_params.get("max_lng"));

        if (!validateBounds(min_lat, max_lat, min_lng, max_lng)) {
            return crow::response(400, createErrorResponse("Invalid boundary values."));
        }
        
        std::string filter_type = req.url_params.get("type") ? req.url_params.get("type") : "";
        auto airports = airportRepository_.fetchAirportsInBounds(min_lat, max_lat, min_lng, max_lng, filter_type);

        nlohmann::json json_airports = nlohmann::json::array();
        for (const auto& airport : airports) {
            json_airports.push_back(airport.toJson());
        }
        return crow::response(200, createSuccessResponse(json_airports));
    } catch (const std::exception&) {
        return crow::response(400, createErrorResponse("Invalid or missing boundary parameters."));
    }
}

crow::response AirportController::searchAirports(const crow::request& req) {
    auto query = req.url_params.get("q");
    if (!query) {
        return crow::response(400, createErrorResponse("Missing search query 'q'"));
    }
    int limit = req.url_params.get("limit") ? std::stoi(req.url_params.get("limit")) : 20;

    auto airports = airportRepository_.searchAirportsByQuery(query, limit);
    nlohmann::json json_airports = nlohmann::json::array();
    for (const auto& airport : airports) {
        json_airports.push_back(airport.toJson());
    }
    return crow::response(200, createSuccessResponse(json_airports));
}

// Helper Methods
bool AirportController::validateBounds(double min_lat, double max_lat, double min_lng, double max_lng) {
    return (min_lat >= -90.0 && max_lat <= 90.0 && min_lat <= max_lat) &&
           (min_lng >= -180.0 && max_lng <= 180.0 && min_lng <= max_lng);
}

nlohmann::json AirportController::createErrorResponse(const std::string& message, int code) {
    return nlohmann::json{{"status", "error"}, {"code", code}, {"message", message}};
}

nlohmann::json AirportController::createSuccessResponse(const nlohmann::json& data) {
    return nlohmann::json{{"status", "success"}, {"data", data}};
}

} // namespace aeronautical