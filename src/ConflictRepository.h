#pragma once

#include <spdlog/spdlog.h>
#include <memory>

namespace aeronautical {

class ConflictRepository {
public:
    ConflictRepository();
    
        // Deletes all existing conflicts for a project before re-analysis
    void deleteByProjectId(int project_id);
    
    // Creates a single new conflict record
    bool create(int project_id, int procedure_id, const std::string& description, const std::string& conflicting_geometry_json);


private:
    std::shared_ptr<spdlog::logger> logger_;
};

} // namespace aeronautical