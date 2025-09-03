#include "FlightProcedureController.h"
#include <json.hpp>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace aeronautical {

FlightProcedureController::FlightProcedureController() 
    : repository_(std::make_unique<FlightProcedureRepository>()) {
    // Fixed spdlog initialization
    try {
        logger_ = spdlog::get("aeronautical");
        if (!logger_) {
            // Create a console logger with color support
            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            logger_ = std::make_shared<spdlog::logger>("aeronautical", console_sink);
            spdlog::register_logger(logger_);
        }
    } catch (const std::exception& e) {
        // Fallback to default logger
        logger_ = spdlog::default_logger();
    }
}

void FlightProcedureController::registerRoutes(crow::SimpleApp& app) {
    // GET /api/procedures
    CROW_ROUTE(app, "/api/procedures")
        .methods(crow::HTTPMethod::GET)
        ([this](const crow::request& req) {
            return getProcedures(req);
        });
    
    // GET /api/procedures/:id
    CROW_ROUTE(app, "/api/procedures/<int>")
        .methods(crow::HTTPMethod::GET)
        ([this](int id) {
            return getProcedure(id);
        });
    
    // GET /api/procedures/code/:code
    CROW_ROUTE(app, "/api/procedures/code/<string>")
        .methods(crow::HTTPMethod::GET)
        ([this](const std::string& code) {
            return getProcedureByCode(code);
        });
    
    // GET /api/procedures/airport/:airport_icao
    CROW_ROUTE(app, "/api/procedures/airport/<string>")
        .methods(crow::HTTPMethod::GET)
        ([this](const std::string& airport_icao) {
            return getProceduresByAirport(airport_icao);
        });
    
    // POST /api/procedures
    CROW_ROUTE(app, "/api/procedures")
        .methods(crow::HTTPMethod::POST)
        ([this](const crow::request& req) {
            return createProcedure(req);
        });
    
    // PUT /api/procedures/:id
    CROW_ROUTE(app, "/api/procedures/<int>")
        .methods(crow::HTTPMethod::PUT)
        ([this](const crow::request& req, int id) {
            return updateProcedure(id, req);
        });
    
    // DELETE /api/procedures/:id
    CROW_ROUTE(app, "/api/procedures/<int>")
        .methods(crow::HTTPMethod::DELETE)
        ([this](int id) {
            return deleteProcedure(id);
        });
    
    // Segment routes
    // GET /api/procedures/:id/segments
    CROW_ROUTE(app, "/api/procedures/<int>/segments")
        .methods(crow::HTTPMethod::GET)
        ([this](int procedure_id) {
            return getProcedureSegments(procedure_id);
        });
    
    // POST /api/procedures/:id/segments
    CROW_ROUTE(app, "/api/procedures/<int>/segments")
        .methods(crow::HTTPMethod::POST)
        ([this](const crow::request& req, int procedure_id) {
            return createProcedureSegment(procedure_id, req);
        });
    
    // PUT /api/procedures/:procedure_id/segments/:segment_id
    CROW_ROUTE(app, "/api/procedures/<int>/segments/<int>")
        .methods(crow::HTTPMethod::PUT)
        ([this](const crow::request& req, int procedure_id, int segment_id) {
            return updateProcedureSegment(procedure_id, segment_id, req);
        });
    
    // DELETE /api/procedures/:procedure_id/segments/:segment_id
    CROW_ROUTE(app, "/api/procedures/<int>/segments/<int>")
        .methods(crow::HTTPMethod::DELETE)
        ([this](int procedure_id, int segment_id) {
            return deleteProcedureSegment(procedure_id, segment_id);
        });
    
    // Protection routes
    // GET /api/procedures/:id/protections
    CROW_ROUTE(app, "/api/procedures/<int>/protections")
        .methods(crow::HTTPMethod::GET)
        ([this](int procedure_id) {
            return getProcedureProtections(procedure_id);
        });
    
    // POST /api/procedures/:id/protections
    CROW_ROUTE(app, "/api/procedures/<int>/protections")
        .methods(crow::HTTPMethod::POST)
        ([this](const crow::request& req, int procedure_id) {
            return createProcedureProtection(procedure_id, req);
        });
    
    // PUT /api/procedures/:procedure_id/protections/:protection_id
    CROW_ROUTE(app, "/api/procedures/<int>/protections/<int>")
        .methods(crow::HTTPMethod::PUT)
        ([this](const crow::request& req, int procedure_id, int protection_id) {
            return updateProcedureProtection(procedure_id, protection_id, req);
        });
    
    // DELETE /api/procedures/:procedure_id/protections/:protection_id
    CROW_ROUTE(app, "/api/procedures/<int>/protections/<int>")
        .methods(crow::HTTPMethod::DELETE)
        ([this](int procedure_id, int protection_id) {
            return deleteProcedureProtection(procedure_id, protection_id);
        });
    
    logger_->info("Flight procedure routes registered");
}

crow::response FlightProcedureController::getProcedures(const crow::request& req) {
    try {
        logger_->info("=== STARTING getProcedures controller method ===");
        
        FlightProcedureFilter filter;
        
        // Parse query parameters (existing code...)
        auto query = crow::query_string(req.url_params);
        
        if (query.get("is_active")) {
            std::string active_str = query.get("is_active");
            filter.is_active = (active_str == "true" || active_str == "1");
            logger_->debug("Parsed is_active filter: {}", *filter.is_active);
        } else {
            // DEFAULT: Set is_active to true if not specified
            filter.is_active = true;
            logger_->debug("Using default is_active = true");
        }
        
        logger_->debug("Calling repository findAll...");
        auto procedures = repository_->findAll(filter);
        logger_->info("Repository returned {} procedures", procedures.size());
        
        int total = repository_->count(filter);
        logger_->debug("Repository count returned: {}", total);
        
        nlohmann::json response;
        response["data"] = nlohmann::json::array();
        
        for (size_t i = 0; i < procedures.size(); i++) {
            try {
                auto proc_json = procedures[i].toJson();
                response["data"].push_back(proc_json);
                logger_->debug("Converted procedure {} to JSON: {}", i, procedures[i].procedure_code);
            } catch (const std::exception& e) {
                logger_->error("Error converting procedure {} to JSON: {}", i, e.what());
                continue;
            }
        }
        
        response["total"] = total;
        response["limit"] = filter.limit;
        response["offset"] = filter.offset;
        
        logger_->info("Final response JSON array size: {}", response["data"].size());
        logger_->debug("Full response: {}", response.dump());
        
        auto crow_response = successResponse(response);
        logger_->info("=== COMPLETED getProcedures - returning HTTP response ===");
        
        return crow_response;
        
    } catch (const std::exception& e) {
        logger_->error("=== EXCEPTION in getProcedures: {} ===", e.what());
        return errorResponse(500, "Internal server error");
    }
}
crow::response FlightProcedureController::getProcedure(int id) {
    try {
        auto procedure = repository_->findById(id);
        
        if (!procedure) {
            return errorResponse(404, "Procedure not found");
        }
        
        nlohmann::json response;
        response["data"] = procedure->toJson();
        
        return successResponse(response);
        
    } catch (const std::exception& e) {
        logger_->error("Failed to get procedure {}: {}", id, e.what());
        return errorResponse(500, "Internal server error");
    }
}

crow::response FlightProcedureController::getProcedureByCode(const std::string& code) {
    try {
        auto procedure = repository_->findByCode(code);
        
        if (!procedure) {
            return errorResponse(404, "Procedure not found");
        }
        
        nlohmann::json response;
        response["data"] = procedure->toJson();
        
        return successResponse(response);
        
    } catch (const std::exception& e) {
        logger_->error("Failed to get procedure by code {}: {}", code, e.what());
        return errorResponse(500, "Internal server error");
    }
}

crow::response FlightProcedureController::getProceduresByAirport(const std::string& airport_icao) {
    try {
        auto procedures = repository_->findByAirport(airport_icao);
        
        nlohmann::json response;
        response["data"] = nlohmann::json::array();
        for (const auto& procedure : procedures) {
            response["data"].push_back(procedure.toJson());
        }
        response["total"] = procedures.size();
        response["airport_icao"] = airport_icao;
        
        return successResponse(response);
        
    } catch (const std::exception& e) {
        logger_->error("Failed to get procedures for airport {}: {}", airport_icao, e.what());
        return errorResponse(500, "Internal server error");
    }
}

crow::response FlightProcedureController::createProcedure(const crow::request& req) {
    try {
        // Check authorization
        std::string authError;
        if (!checkAuthorization(req, authError)) {
            return errorResponse(401, authError);
        }
        
        // Parse request body
        auto body = nlohmann::json::parse(req.body);
        
        // Validate input
        std::string validationError;
        if (!validateProcedureInput(body, validationError)) {
            return errorResponse(400, validationError);
        }
        
        // Create procedure from JSON
        FlightProcedure procedure = FlightProcedure::fromJson(body);
        
        // Save to database
        auto created = repository_->create(procedure);
        
        nlohmann::json response;
        response["data"] = created.toJson();
        response["message"] = "Procedure created successfully";
        
        logger_->info("Created procedure: {} - {}", created.procedure_code, created.name);
        
        return crow::response(201, response.dump());
        
    } catch (const nlohmann::json::exception& e) {
        logger_->error("Invalid JSON in create procedure request: {}", e.what());
        return errorResponse(400, "Invalid JSON format");
    } catch (const std::exception& e) {
        logger_->error("Failed to create procedure: {}", e.what());
        return errorResponse(500, "Internal server error");
    }
}

crow::response FlightProcedureController::updateProcedure(int id, const crow::request& req) {
    try {
        // Check authorization
        std::string authError;
        if (!checkAuthorization(req, authError)) {
            return errorResponse(401, authError);
        }
        
        // Check if procedure exists
        auto existing = repository_->findById(id);
        if (!existing) {
            return errorResponse(404, "Procedure not found");
        }
        
        // Parse request body
        auto body = nlohmann::json::parse(req.body);
        
        // Validate input
        std::string validationError;
        if (!validateProcedureInput(body, validationError)) {
            return errorResponse(400, validationError);
        }
        
        // Update procedure from JSON
        FlightProcedure procedure = FlightProcedure::fromJson(body);
        procedure.id = id; // Ensure ID is not changed
        
        // Save to database
        bool updated = repository_->update(id, procedure);
        
        if (!updated) {
            return errorResponse(500, "Failed to update procedure");
        }
        
        // Get updated procedure
        auto updatedProcedure = repository_->findById(id);
        
        nlohmann::json response;
        response["data"] = updatedProcedure->toJson();
        response["message"] = "Procedure updated successfully";
        
        logger_->info("Updated procedure: {} - {}", updatedProcedure->procedure_code, updatedProcedure->name);
        
        return successResponse(response);
        
    } catch (const nlohmann::json::exception& e) {
        logger_->error("Invalid JSON in update procedure request: {}", e.what());
        return errorResponse(400, "Invalid JSON format");
    } catch (const std::exception& e) {
        logger_->error("Failed to update procedure {}: {}", id, e.what());
        return errorResponse(500, "Internal server error");
    }
}

crow::response FlightProcedureController::deleteProcedure(int id) {
    try {
        // Check authorization
        std::string authError;
        if (!checkAuthorization(crow::request(), authError)) {
            return errorResponse(401, authError);
        }
        
        bool deleted = repository_->deleteById(id);
        
        if (!deleted) {
            return errorResponse(404, "Procedure not found");
        }
        
        nlohmann::json response;
        response["message"] = "Procedure deleted successfully";
        
        logger_->info("Deleted procedure with ID: {}", id);
        
        return successResponse(response);
        
    } catch (const std::exception& e) {
        logger_->error("Failed to delete procedure {}: {}", id, e.what());
        return errorResponse(500, "Internal server error");
    }
}

crow::response FlightProcedureController::getProcedureSegments(int procedure_id) {
        // FIXED: Just return empty segments instead of querying non-existent table
            std::vector<ProcedureSegment> segments;
    logger_->debug("getSegments called for procedure {} - returning empty (segments table removed)", procedure_id);
    // Convert to JSON array
    nlohmann::json j = nlohmann::json::array();
    for (const auto& segment : segments) {
        j.push_back(segment.toJson());
    }
    
    return crow::response(200, j.dump());
    // try {
    //     auto segments = repository_->getSegments(procedure_id);
        
    //     nlohmann::json response;
    //     response["data"] = nlohmann::json::array();
    //     for (const auto& segment : segments) {
    //         response["data"].push_back(segment.toJson());
    //     }
    //     response["total"] = segments.size();
    //     response["procedure_id"] = procedure_id;
        
    //     return successResponse(response);
        
    // } catch (const std::exception& e) {
    //     logger_->error("Failed to get segments for procedure {}: {}", procedure_id, e.what());
    //     return errorResponse(500, "Internal server error");
    // }
}

crow::response FlightProcedureController::getProcedureProtections(int procedure_id) {
    std::vector<ProcedureProtection> protections;
    logger_->debug("getProtections called for procedure {} - returning empty (protections table removed)", procedure_id);
    // Convert to JSON array
    nlohmann::json j = nlohmann::json::array();
    for (const auto& segment : protections) {
        j.push_back(segment.toJson());
    }
    
    return crow::response(200, j.dump());
    // try {
    //     auto protections = repository_->getProtections(procedure_id);
        
    //     nlohmann::json response;
    //     response["data"] = nlohmann::json::array();
    //     for (const auto& protection : protections) {
    //         response["data"].push_back(protection.toJson());
    //     }
    //     response["total"] = protections.size();
    //     response["procedure_id"] = procedure_id;
        
    //     return successResponse(response);
        
    // } catch (const std::exception& e) {
    //     logger_->error("Failed to get protections for procedure {}: {}", procedure_id, e.what());
    //     return errorResponse(500, "Internal server error");
    // }
}

// Placeholder implementations for segment and protection CRUD operations
crow::response FlightProcedureController::createProcedureSegment(int procedure_id, const crow::request& req) {
    return errorResponse(501, "Create segment operation not implemented yet");
}

crow::response FlightProcedureController::updateProcedureSegment(int procedure_id, int segment_id, const crow::request& req) {
    return errorResponse(501, "Update segment operation not implemented yet");
}

crow::response FlightProcedureController::deleteProcedureSegment(int procedure_id, int segment_id) {
    return errorResponse(501, "Delete segment operation not implemented yet");
}

crow::response FlightProcedureController::createProcedureProtection(int procedure_id, const crow::request& req) {
    return errorResponse(501, "Create protection operation not implemented yet");
}

crow::response FlightProcedureController::updateProcedureProtection(int procedure_id, int protection_id, const crow::request& req) {
    return errorResponse(501, "Update protection operation not implemented yet");
}

crow::response FlightProcedureController::deleteProcedureProtection(int procedure_id, int protection_id) {
    return errorResponse(501, "Delete protection operation not implemented yet");
}

crow::response FlightProcedureController::errorResponse(int code, const std::string& message) {
    nlohmann::json response;
    response["error"] = true;
    response["message"] = message;
    
    crow::response res(code, response.dump());
    res.add_header("Content-Type", "application/json");
    return res;
}

crow::response FlightProcedureController::successResponse(const nlohmann::json& data) {
    crow::response res(200, data.dump());
    res.add_header("Content-Type", "application/json");
    return res;
}

bool FlightProcedureController::validateProcedureInput(const nlohmann::json& input, std::string& error) {
    // Required fields
    if (!input.contains("procedure_code") || input["procedure_code"].get<std::string>().empty()) {
        error = "Procedure code is required";
        return false;
    }
    
    if (!input.contains("name") || input["name"].get<std::string>().empty()) {
        error = "Procedure name is required";
        return false;
    }
    
    if (!input.contains("type") || input["type"].get<std::string>().empty()) {
        error = "Procedure type is required";
        return false;
    }
    
    if (!input.contains("airport_icao") || input["airport_icao"].get<std::string>().empty()) {
        error = "Airport ICAO code is required";
        return false;
    }
    
    // Validate procedure type
    std::string type = input["type"];
    if (type != "SID" && type != "STAR" && type != "APPROACH" && 
        type != "DEPARTURE" && type != "ARRIVAL") {
        error = "Invalid procedure type. Must be SID, STAR, APPROACH, DEPARTURE, or ARRIVAL";
        return false;
    }
    
    // Validate airport ICAO code format (4 letters)
    std::string icao = input["airport_icao"];
    if (icao.length() != 4) {
        error = "Airport ICAO code must be 4 characters";
        return false;
    }
    
    return true;
}

bool FlightProcedureController::validateSegmentInput(const nlohmann::json& input, std::string& error) {
    // Required fields
    if (!input.contains("segment_order")) {
        error = "Segment order is required";
        return false;
    }
    
    if (!input.contains("trajectory_geometry") || input["trajectory_geometry"].get<std::string>().empty()) {
        error = "Trajectory geometry is required";
        return false;
    }
    
    // Validate segment order is positive
    int order = input["segment_order"];
    if (order < 1) {
        error = "Segment order must be positive";
        return false;
    }
    
    // Validate trajectory geometry is valid GeoJSON
    try {
        std::string geom = input["trajectory_geometry"];
        if (geom.front() == '{') {
            // Try to parse as JSON
            nlohmann::json::parse(geom);
        }
    } catch (...) {
        error = "Invalid trajectory geometry format";
        return false;
    }
    
    return true;
}

bool FlightProcedureController::validateProtectionInput(const nlohmann::json& input, std::string& error) {
    // Required fields
    if (!input.contains("protection_name") || input["protection_name"].get<std::string>().empty()) {
        error = "Protection name is required";
        return false;
    }
    
    if (!input.contains("protection_type") || input["protection_type"].get<std::string>().empty()) {
        error = "Protection type is required";
        return false;
    }
    
    if (!input.contains("protection_geometry") || input["protection_geometry"].get<std::string>().empty()) {
        error = "Protection geometry is required";
        return false;
    }
    
    // Validate protection geometry is valid GeoJSON
    try {
        std::string geom = input["protection_geometry"];
        if (geom.front() == '{') {
            // Try to parse as JSON
            nlohmann::json::parse(geom);
        }
    } catch (...) {
        error = "Invalid protection geometry format";
        return false;
    }
    
    return true;
}

bool FlightProcedureController::checkAuthorization(const crow::request& req, std::string& error) {
    // TODO: Implement proper JWT token validation
    // For now, just check for presence of Authorization header
    
    auto auth_header = req.get_header_value("Authorization");
    if (auth_header.empty()) {
        error = "Authorization header required";
        return false;
    }
    
    // Fixed: Use C++11/14 compatible substring check instead of starts_with (C++20)
    if (auth_header.length() < 7 || auth_header.substr(0, 7) != "Bearer ") {
        error = "Invalid authorization format";
        return false;
    }
    
    // TODO: Validate JWT token
    // For now, accept any bearer token
    
    return true;
}
}