#include <crow.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <json.hpp>

#include "DatabaseManager.h"
#include "ProjectController.h"
#include "FlightProcedureController.h"
#include "AirportController.h"

void setupLogger() {
    // Console sink
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_level(spdlog::level::debug);
    
    // File sink
    auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        "logs/aeronautical.log", 1048576 * 5, 3);
    file_sink->set_level(spdlog::level::info);
    
    // Create logger with both sinks
    std::vector<spdlog::sink_ptr> sinks {console_sink, file_sink};
    auto logger = std::make_shared<spdlog::logger>("aeronautical", sinks.begin(), sinks.end());
    logger->set_level(spdlog::level::debug);
    
    // Register as default logger
    spdlog::register_logger(logger);
    spdlog::set_default_logger(logger);
}

void setupCORS(crow::SimpleApp& app) {
    // CORS middleware
    struct CORSHandler {
        struct context {};
        
        void before_handle(crow::request& req, crow::response& res, context& ctx) {
            // Nothing to do before
        }
        
        void after_handle(crow::request& req, crow::response& res, context& ctx) {
            res.add_header("Access-Control-Allow-Origin", "*");
            res.add_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
            res.add_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
            res.add_header("Access-Control-Max-Age", "3600");
        }
    };
    
    // Handle OPTIONS requests
    CROW_ROUTE(app, "/api/<path>")
        .methods(crow::HTTPMethod::OPTIONS)
        ([](const std::string& path) {
            crow::response res(204);
            res.add_header("Access-Control-Allow-Origin", "*");
            res.add_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
            res.add_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
            return res;
        });
}

int main(int argc, char* argv[]) {
    try {
        // Setup logging
        setupLogger();
        auto logger = spdlog::get("aeronautical");
        logger->info("===== Aeronautical Platform Backend Starting =====");
        
        // Load configuration (can be from file or environment variables)
        std::string db_host = std::getenv("DB_HOST") ? std::getenv("DB_HOST") : "localhost";
        int db_port = std::getenv("DB_PORT") ? std::stoi(std::getenv("DB_PORT")) : 33060;
        std::string db_user = std::getenv("DB_USER") ? std::getenv("DB_USER") : "root";
        std::string db_pass = std::getenv("DB_PASS") ? std::getenv("DB_PASS") : "oper";
        std::string db_name = std::getenv("DB_NAME") ? std::getenv("DB_NAME") : "aeronautical_platform";
        int server_port = std::getenv("SERVER_PORT") ? std::stoi(std::getenv("SERVER_PORT")) : 8081;
        
        // Initialize database
        logger->info("Connecting to database at {}:{}/{}", db_host, db_port, db_name);
        aeronautical::DatabaseManager::getInstance().initialize(
            db_host, db_port, db_user, db_pass, db_name
        );
        
        // Create Crow application
        crow::SimpleApp app;
        
        // Setup CORS
        setupCORS(app);
        
        // Health check endpoint
        CROW_ROUTE(app, "/api/health")
            .methods(crow::HTTPMethod::GET)
            ([]() {
                nlohmann::json response;
                response["status"] = "healthy";
                response["service"] = "aeronautical-platform-backend";
                response["version"] = "1.0.0";
                response["timestamp"] = std::time(nullptr);
                
                crow::response res(200, response.dump());
                res.add_header("Content-Type", "application/json");
                return res;
            });
        
        // Register controllers
        aeronautical::ProjectController projectController;
        projectController.registerRoutes(app);
        logger->info("Project controller registered");
        
        aeronautical::FlightProcedureController procedureController;
        procedureController.registerRoutes(app);
        logger->info("Flight procedure controller registered");


        // Register the new ConflictController routes
        aeronautical::ConflictController::getInstance().registerRoutes(app);
        logger->info("Conflict controller registered");

        aeronautical::AirportController airportController;
        airportController.registerRoutes(app);
        logger->info("Airport controller registered");

        
        // TODO: Add more controllers as needed
        // aeronautical::GeometryController geometryController;
        // geometryController.registerRoutes(app);
        
        // aeronautical::ConflictController conflictController;
        // conflictController.registerRoutes(app);
        
        // Start server
        logger->info("Starting server on port {}", server_port);
        app.port(server_port)
           .multithreaded()
           .run();
        
    } catch (const std::exception& e) {
        if (auto logger = spdlog::get("aeronautical")) {
            logger->critical("Fatal error: {}", e.what());
        } else {
            std::cerr << "Fatal error: " << e.what() << std::endl;
        }
        return 1;
    }
    
    return 0;
}