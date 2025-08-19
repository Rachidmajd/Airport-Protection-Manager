#pragma once

#include <crow.h>
#include "FlightProcedureRepository.h"
#include <memory>
#include <spdlog/spdlog.h>

namespace aeronautical {

class FlightProcedureController {
public:
    FlightProcedureController();
    ~FlightProcedureController() = default;
    
    void registerRoutes(crow::SimpleApp& app);
    
private:
    std::unique_ptr<FlightProcedureRepository> repository_;
    std::shared_ptr<spdlog::logger> logger_;
    
    // Route handlers
    crow::response getProcedures(const crow::request& req);
    crow::response getProcedure(int id);
    crow::response getProcedureByCode(const std::string& code);
    crow::response getProceduresByAirport(const std::string& airport_icao);
    crow::response createProcedure(const crow::request& req);
    crow::response updateProcedure(int id, const crow::request& req);
    crow::response deleteProcedure(int id);
    
    // Segment route handlers
    crow::response getProcedureSegments(int procedure_id);
    crow::response createProcedureSegment(int procedure_id, const crow::request& req);
    crow::response updateProcedureSegment(int procedure_id, int segment_id, const crow::request& req);
    crow::response deleteProcedureSegment(int procedure_id, int segment_id);
    
    // Protection route handlers
    crow::response getProcedureProtections(int procedure_id);
    crow::response createProcedureProtection(int procedure_id, const crow::request& req);
    crow::response updateProcedureProtection(int procedure_id, int protection_id, const crow::request& req);
    crow::response deleteProcedureProtection(int procedure_id, int protection_id);
    
    // Helper methods
    crow::response errorResponse(int code, const std::string& message);
    crow::response successResponse(const nlohmann::json& data);
    bool validateProcedureInput(const nlohmann::json& input, std::string& error);
    bool validateSegmentInput(const nlohmann::json& input, std::string& error);
    bool validateProtectionInput(const nlohmann::json& input, std::string& error);
    bool checkAuthorization(const crow::request& req, std::string& error);
};

} // namespace aeronautical