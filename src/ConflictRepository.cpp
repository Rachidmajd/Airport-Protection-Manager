#include "ConflictRepository.h"
#include "DatabaseManager.h"
#include <sstream>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace aeronautical {

ConflictRepository::ConflictRepository() {
    // Logger setup...
    try {
        logger_ = spdlog::get("aeronautical");
        if (!logger_) {
            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            logger_ = std::make_shared<spdlog::logger>("aeronautical", console_sink);
            spdlog::register_logger(logger_);
        }
    } catch (const std::exception& e) {
        logger_ = spdlog::default_logger();
    }
}

void ConflictRepository::deleteByProjectId(int project_id) {
    try {
        auto& db = DatabaseManager::getInstance();
        std::string query = "DELETE FROM conflicts WHERE project_id = " + std::to_string(project_id);
        db.executeQuery(query);
    } catch (const std::exception& err) {
        logger_->error("Failed to delete old conflicts for project {}: {}", project_id, err.what());
    }
}

bool ConflictRepository::create(int project_id, int procedure_id, const std::string& description, const std::string& conflicting_geometry_json) {
    try {
        auto& db = DatabaseManager::getInstance();
        
        // Escape strings for SQL
        std::string escaped_desc = description;
        // ... (add proper escaping if needed)
        std::string escaped_geom = conflicting_geometry_json;
        // ...

        std::stringstream query;
        query << "INSERT INTO conflicts (project_id, flight_procedure_id, description, conflicting_geometry) "
              << "VALUES ("
              << project_id << ", "
              << procedure_id << ", "
              << "'" << escaped_desc << "', "
              << "ST_GeomFromGeoJSON('" << escaped_geom << "')"
              << ");";

        return db.executeQuery(query.str());

    } catch (const std::exception& err) {
        logger_->error("Failed to create conflict for project {}: {}", project_id, err.what());
        return false;
    }
}

} // namespace aeronautical