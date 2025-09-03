#include "ProjectRepository.h"
#include "DatabaseManager.h"
#include <sstream>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace aeronautical {

ProjectRepository::ProjectRepository() {
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

#ifdef USE_MYSQL_C_API

std::string ProjectRepository::buildSelectQuery() const {
      return "SELECT p.id, p.project_code, p.title, p.description, p.demander_id, "
           "p.demander_name, p.demander_organization, p.demander_email, "
           "p.demander_phone, p.status, p.priority, p.operation_type, "
           "p.altitude_min, p.altitude_max, p.start_date, p.end_date, "
           "p.assigned_reviewer_id, p.review_deadline, p.approval_date, "
           "p.rejection_reason, p.comment, p.internal_notes, "
           "p.created_at, p.updated_at, "
           "0 as doc_count, 0 as geo_count, 0 as conflict_count " // Placeholder values
           "FROM projects p";
}

std::string ProjectRepository::buildCountQuery() const {
    return "SELECT COUNT(*) FROM projects p";
}

std::vector<Project> ProjectRepository::findAll(const ProjectFilter& filter) {
    std::vector<Project> projects;
    
    try {
        auto& db = DatabaseManager::getInstance();
        
        std::stringstream query;
        query << buildSelectQuery() << " WHERE 1=1";
        
        if (filter.status) {
            query << " AND p.status = '" << statusToString(*filter.status) << "'";
        }
        
        if (filter.demander_id) {
            query << " AND p.demander_id = " << *filter.demander_id;
        }
        
        if (filter.priority) {
            query << " AND p.priority = '" << priorityToString(*filter.priority) << "'";
        }
        
        query << " ORDER BY p.created_at DESC";
        query << " LIMIT " << filter.limit << " OFFSET " << filter.offset;
        
        logger_->debug("About to execute query: {}", query.str());
        
        MYSQL_RES* result = db.executeSelectQuery(query.str());
        if (!result) {
            logger_->warn("Query returned no result set, returning empty projects list");
            return projects; // Return empty list instead of throwing
        }
        
        unsigned long num_rows = mysql_num_rows(result);
        logger_->debug("Processing {} rows from projects query", num_rows);
        
        MYSQL_ROW row;
        int rowCount = 0;
        while ((row = mysql_fetch_row(result))) {
            try {
                unsigned long* lengths = mysql_fetch_lengths(result);
                if (!lengths) {
                    logger_->warn("Failed to get field lengths for row {}", rowCount);
                    continue;
                }
                
                Project project = rowToProject(row, lengths);
                projects.push_back(project);
                rowCount++;
            } catch (const std::exception& e) {
                logger_->error("Error processing project row {}: {}", rowCount, e.what());
                // Continue with next row
            }
        }
        
        mysql_free_result(result);
        logger_->debug("Successfully processed {} projects", projects.size());
        
    } catch (const std::exception& err) {
        logger_->error("Failed to fetch projects: {}", err.what());
        // Return empty list instead of throwing
        return std::vector<Project>();
    }
    
    return projects;
}

std::optional<Project> ProjectRepository::findById(int id) {
    try {
        auto& db = DatabaseManager::getInstance();
        
        std::stringstream query;
        query << buildSelectQuery() << " WHERE p.id = " << id;
        
        MYSQL_RES* result = db.executeSelectQuery(query.str());
        if (!result) {
            throw std::runtime_error("Failed to execute query");
        }
        
        MYSQL_ROW row = mysql_fetch_row(result);
        if (row) {
            unsigned long* lengths = mysql_fetch_lengths(result);
            Project project = rowToProject(row, lengths);
            mysql_free_result(result);
            return project;
        }
        
        mysql_free_result(result);
    } catch (const std::exception& err) {
        logger_->error("Failed to find project by id {}: {}", id, err.what());
        throw;
    }
    
    return std::nullopt;
}

std::optional<Project> ProjectRepository::findByCode(const std::string& code) {
    try {
        auto& db = DatabaseManager::getInstance();
        MYSQL* conn = db.getConnection();
        
        std::stringstream query;
        query << buildSelectQuery() << " WHERE p.project_code = '" << code << "'";
        
        MYSQL_RES* result = db.executeSelectQuery(query.str());
        if (!result) {
            throw std::runtime_error("Failed to execute query");
        }
        
        MYSQL_ROW row = mysql_fetch_row(result);
        if (row) {
            unsigned long* lengths = mysql_fetch_lengths(result);
            Project project = rowToProject(row, lengths);
            mysql_free_result(result);
            return project;
        }
        
        mysql_free_result(result);
    } catch (const std::exception& err) {
        logger_->error("Failed to find project by code {}: {}", code, err.what());
        throw;
    }
    
    return std::nullopt;
}

// Add this new function at the end of the file
std::optional<std::string> ProjectRepository::findGeometriesByProjectId(int project_id) {
    try {
        auto& db = DatabaseManager::getInstance();
        std::string query = "SELECT geometry_data FROM project_geometries WHERE project_id = " + std::to_string(project_id) + " AND is_primary = 1 LIMIT 1";
        
        MYSQL_RES* result = db.executeSelectQuery(query);
        if (result && mysql_num_rows(result) > 0) {
            MYSQL_ROW row = mysql_fetch_row(result);
            if (row && row[0]) {
                std::string geometry_json(row[0]);
                mysql_free_result(result);
                return geometry_json;
            }
        }
        if (result) {
            mysql_free_result(result);
        }
    } catch (const std::exception& err) {
        logger_->error("Failed to find geometries for project {}: {}", project_id, err.what());
        throw;
    }
    
    return std::nullopt;
}

Project ProjectRepository::create(const Project& project) {
    try {
        auto& db = DatabaseManager::getInstance();
        
        // Generate project code if not provided
        std::string projectCode = project.project_code.empty() ? 
            db.generateProjectCode() : project.project_code;
        
        std::stringstream query;
        query << "INSERT INTO projects "
              << "(project_code, title, description, demander_id, demander_name, "
              << "demander_organization, demander_email, demander_phone, status, priority, "
              << "operation_type, altitude_min, altitude_max, start_date, end_date, "
              << "comment, internal_notes) "
              << "VALUES (";
        
        query << "'" << projectCode << "', ";
        query << "'" << project.title << "', ";
        query << (project.description ? ("'" + *project.description + "'") : "NULL") << ", ";
        query << project.demander_id << ", ";
        query << "'" << project.demander_name << "', ";
        query << (project.demander_organization ? ("'" + *project.demander_organization + "'") : "NULL") << ", ";
        query << "'" << project.demander_email << "', ";
        query << (project.demander_phone ? ("'" + *project.demander_phone + "'") : "NULL") << ", ";
        query << "'" << statusToString(project.status) << "', ";
        query << "'" << priorityToString(project.priority) << "', ";
        query << (project.operation_type ? ("'" + *project.operation_type + "'") : "NULL") << ", ";
        query << (project.altitude_min ? std::to_string(*project.altitude_min) : "NULL") << ", ";
        query << (project.altitude_max ? std::to_string(*project.altitude_max) : "NULL") << ", ";
        query << (project.start_date ? ("'" + timePointToString(*project.start_date) + "'") : "NULL") << ", ";
        query << (project.end_date ? ("'" + timePointToString(*project.end_date) + "'") : "NULL") << ", ";
        query << (project.comment ? ("'" + *project.comment + "'") : "NULL") << ", ";
        query << (project.internal_notes ? ("'" + *project.internal_notes + "'") : "NULL");
        query << ")";
        
        if (!db.executeQuery(query.str())) {
            throw std::runtime_error("Failed to execute insert query");
        }
        
        // Get the inserted ID
        MYSQL* conn = db.getConnection();
        my_ulonglong insertedId = mysql_insert_id(conn);
        
        logger_->info("Created project with ID {} and code {}", insertedId, projectCode);
        
        // Return the created project
        auto created = findById(insertedId);
        if (created) {
            return *created;
        }
        
        throw std::runtime_error("Failed to retrieve created project");
        
    } catch (const std::exception& err) {
        logger_->error("Failed to create project: {}", err.what());
        throw;
    }
}

bool ProjectRepository::update(int id, const Project& project) {
    try {
        auto& db = DatabaseManager::getInstance();
        
        std::stringstream query;
        query << "UPDATE projects SET ";
        query << "title = '" << project.title << "', ";
        query << "description = " << (project.description ? ("'" + *project.description + "'") : "NULL") << ", ";
        query << "demander_name = '" << project.demander_name << "', ";
        query << "demander_organization = " << (project.demander_organization ? ("'" + *project.demander_organization + "'") : "NULL") << ", ";
        query << "demander_email = '" << project.demander_email << "', ";
        query << "demander_phone = " << (project.demander_phone ? ("'" + *project.demander_phone + "'") : "NULL") << ", ";
        query << "status = '" << statusToString(project.status) << "', ";
        query << "priority = '" << priorityToString(project.priority) << "', ";
        query << "operation_type = " << (project.operation_type ? ("'" + *project.operation_type + "'") : "NULL") << ", ";
        query << "altitude_min = " << (project.altitude_min ? std::to_string(*project.altitude_min) : "NULL") << ", ";
        query << "altitude_max = " << (project.altitude_max ? std::to_string(*project.altitude_max) : "NULL") << ", ";
        query << "start_date = " << (project.start_date ? ("'" + timePointToString(*project.start_date) + "'") : "NULL") << ", ";
        query << "end_date = " << (project.end_date ? ("'" + timePointToString(*project.end_date) + "'") : "NULL") << ", ";
        query << "assigned_reviewer_id = " << (project.assigned_reviewer_id ? std::to_string(*project.assigned_reviewer_id) : "NULL") << ", ";
        query << "review_deadline = " << (project.review_deadline ? ("'" + timePointToString(*project.review_deadline) + "'") : "NULL") << ", ";
        query << "approval_date = " << (project.approval_date ? ("'" + timePointToString(*project.approval_date) + "'") : "NULL") << ", ";
        query << "rejection_reason = " << (project.rejection_reason ? ("'" + *project.rejection_reason + "'") : "NULL") << ", ";
        query << "comment = " << (project.comment ? ("'" + *project.comment + "'") : "NULL") << ", ";
        query << "internal_notes = " << (project.internal_notes ? ("'" + *project.internal_notes + "'") : "NULL") << ", ";
        query << "updated_at = NOW() ";
        query << "WHERE id = " << id;
        
        bool result = db.executeQuery(query.str());
        
        if (result) {
            logger_->info("Updated project with ID {}", id);
        } else {
            logger_->warn("No project found with ID {} to update", id);
        }
        
        return result;
        
    } catch (const std::exception& err) {
        logger_->error("Failed to update project {}: {}", id, err.what());
        throw;
    }
}

bool ProjectRepository::deleteById(int id) {
    try {
        auto& db = DatabaseManager::getInstance();
        
        std::stringstream query;
        query << "DELETE FROM projects WHERE id = " << id;
        
        bool result = db.executeQuery(query.str());
        
        if (result) {
            logger_->info("Deleted project with ID {}", id);
        } else {
            logger_->warn("No project found with ID {} to delete", id);
        }
        
        return result;
        
    } catch (const std::exception& err) {
        logger_->error("Failed to delete project {}: {}", id, err.what());
        throw;
    }
}

// int ProjectRepository::count(const ProjectFilter& filter) {
//     try {
//         auto& db = DatabaseManager::getInstance();
        
//         std::stringstream query;
//         query << buildCountQuery() << " WHERE 1=1";
        
//         if (filter.status) {
//             query << " AND status = '" << statusToString(*filter.status) << "'";
//         }
        
//         if (filter.demander_id) {
//             query << " AND demander_id = " << *filter.demander_id;
//         }
        
//         if (filter.priority) {
//             query << " AND priority = '" << priorityToString(*filter.priority) << "'";
//         }
        
//         MYSQL_RES* result = db.executeSelectQuery(query.str());
//         if (!result) {
//             throw std::runtime_error("Failed to execute count query");
//         }
        
//         MYSQL_ROW row = mysql_fetch_row(result);
//         int count = 0;
//         if (row && row[0]) {
//             count = std::atoi(row[0]);
//         }
        
//         mysql_free_result(result);
//         return count;
        
//     } catch (const std::exception& err) {
//         logger_->error("Failed to count projects: {}", err.what());
//         throw;
//     }
// }

int ProjectRepository::count(const ProjectFilter& filter) {
    try {
        auto& db = DatabaseManager::getInstance();
        
        std::stringstream query;
        query << "SELECT COUNT(*) FROM projects p WHERE 1=1";
        
        if (filter.status) {
            query << " AND p.status = '" << statusToString(*filter.status) << "'";
        }
        
        if (filter.demander_id) {
            query << " AND p.demander_id = " << *filter.demander_id;
        }
        
        if (filter.priority) {
            query << " AND p.priority = '" << priorityToString(*filter.priority) << "'";
        }
        
        logger_->debug("Executing project count query: {}", query.str());
        
        MYSQL_RES* result = db.executeSelectQuery(query.str());
        if (!result) {
            logger_->error("Project count query failed");
            return 0;
        }
        
        MYSQL_ROW row = mysql_fetch_row(result);
        int count = 0;
        if (row && row[0]) {
            count = std::atoi(row[0]);
            logger_->debug("Project count query returned: {}", count);
        } else {
            logger_->warn("Project count query returned NULL result");
        }
        
        mysql_free_result(result);
        return count;
        
    } catch (const std::exception& err) {
        logger_->error("Failed to count projects: {}", err.what());
        return 0;
    }
}

Project ProjectRepository::rowToProject(MYSQL_ROW row, unsigned long* lengths) {
    Project p;
    
    int col = 0;
    p.id = row[col] ? std::atoi(row[col]) : 0; col++;
    p.project_code = row[col] ? std::string(row[col]) : ""; col++;
    p.title = row[col] ? std::string(row[col]) : ""; col++;
    if (row[col]) p.description = std::string(row[col]); col++;
    p.demander_id = row[col] ? std::atoi(row[col]) : 0; col++;
    p.demander_name = row[col] ? std::string(row[col]) : ""; col++;
    if (row[col]) p.demander_organization = std::string(row[col]); col++;
    p.demander_email = row[col] ? std::string(row[col]) : ""; col++;
    if (row[col]) p.demander_phone = std::string(row[col]); col++;
    p.status = row[col] ? stringToStatus(std::string(row[col])) : ProjectStatus::Created; col++;
    p.priority = row[col] ? stringToPriority(std::string(row[col])) : ProjectPriority::Normal; col++;
    if (row[col]) p.operation_type = std::string(row[col]); col++;
    if (row[col]) p.altitude_min = std::atoi(row[col]); col++;
    if (row[col]) p.altitude_max = std::atoi(row[col]); col++;
    if (row[col]) p.start_date = stringToTimePoint(std::string(row[col])); col++;
    if (row[col]) p.end_date = stringToTimePoint(std::string(row[col])); col++;
    if (row[col]) p.assigned_reviewer_id = std::atoi(row[col]); col++;
    if (row[col]) p.review_deadline = stringToTimePoint(std::string(row[col])); col++;
    if (row[col]) p.approval_date = stringToTimePoint(std::string(row[col])); col++;
    if (row[col]) p.rejection_reason = std::string(row[col]); col++;
    if (row[col]) p.comment = std::string(row[col]); col++;
    if (row[col]) p.internal_notes = std::string(row[col]); col++;
    
    // created_at and updated_at
    if (row[col]) p.created_at = stringToTimePoint(std::string(row[col])); col++;
    if (row[col]) p.updated_at = stringToTimePoint(std::string(row[col])); col++;
    
    // Additional computed fields if present
    if (row[col]) p.document_count = std::atoi(row[col]); col++;
    if (row[col]) p.geometry_count = std::atoi(row[col]); col++;
    if (row[col]) p.conflict_count = std::atoi(row[col]); col++;
    
    return p;
}

#else
// MySQL Connector/C++ implementation (original code would go here)
// For now, throw an error since we're using MySQL C API
#error "MySQL Connector/C++ code not implemented in this version"
#endif

} // namespace aeronautical
