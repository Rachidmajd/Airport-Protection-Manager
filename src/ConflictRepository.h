#pragma once

#include <spdlog/spdlog.h>
#include <memory>
#include "FlightProcedure.h"
#include <mysql/mysql.h> 

namespace aeronautical {

class ConflictRepository {
public:
    ConflictRepository();
    
        // Deletes all existing conflicts for a project before re-analysis
    void deleteByProjectId(int project_id);
    std::vector<Conflict> findByProjectId(int project_id); // Add this function declaration

    
    // Creates a single new conflict record
    bool create(int project_id, int procedure_id, const std::string& description, const std::string& conflicting_geometry_json);
    Conflict rowToConflict(MYSQL_ROW row);

private:
    std::shared_ptr<spdlog::logger> logger_;
};

} // namespace aeronautical