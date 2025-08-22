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
        logger_->debug("Deleted existing conflicts for project {}", project_id);
    } catch (const std::exception& err) {
        logger_->error("Failed to delete old conflicts for project {}: {}", project_id, err.what());
    }
}

bool ConflictRepository::create(int project_id, int procedure_id, const std::string& description, const std::string& conflicting_geometry_json) {
    try {
        auto& db = DatabaseManager::getInstance();
        
        // Properly escape strings for SQL
        std::string escaped_desc = description;
        size_t pos = 0;
        while ((pos = escaped_desc.find("'", pos)) != std::string::npos) {
            escaped_desc.replace(pos, 1, "\\'");
            pos += 2;
        }
        
        std::string escaped_geom = conflicting_geometry_json;
        pos = 0;
        while ((pos = escaped_geom.find("'", pos)) != std::string::npos) {
            escaped_geom.replace(pos, 1, "\\'");
            pos += 2;
        }

        std::stringstream query;
        
        // Check if the conflicts table uses a geometry column or just stores JSON
        // For MySQL without PostGIS, we'll store the geometry as JSON text
        query << "INSERT INTO conflicts (project_id, flight_procedure_id, description, conflicting_geometry) "
              << "VALUES ("
              << project_id << ", "
              << procedure_id << ", "
              << "'" << escaped_desc << "', ";
        
        // Try to use ST_GeomFromGeoJSON if available, otherwise store as text
        // First, let's check if we can use spatial functions
        bool use_spatial = false;
        
        MYSQL_RES* result = db.executeSelectQuery("SELECT VERSION()");
        if (result) {
            mysql_free_result(result);
            // Check if ST_GeomFromGeoJSON exists
            result = db.executeSelectQuery("SHOW FUNCTION STATUS WHERE name = 'ST_GeomFromGeoJSON'");
            if (result && mysql_num_rows(result) > 0) {
                use_spatial = true;
            }
            if (result) mysql_free_result(result);
        }
        
        if (use_spatial) {
            query << "ST_GeomFromGeoJSON('" << escaped_geom << "')";
        } else {
            // Store as text if spatial functions not available
            query << "'" << escaped_geom << "'";
        }
        
        query << ");";
        
        logger_->debug("Executing conflict insert query: {}", query.str());
        
        bool success = db.executeQuery(query.str());
        
        if (success) {
            logger_->info("Successfully created conflict record for project {} and procedure {}", 
                         project_id, procedure_id);
        } else {
            logger_->error("Failed to insert conflict record for project {} and procedure {}", 
                          project_id, procedure_id);
        }
        
        return success;

    } catch (const std::exception& err) {
        logger_->error("Exception in create conflict for project {}: {}", project_id, err.what());
        return false;
    }
}

} // namespace aeronautical