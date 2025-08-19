#pragma once

#include "Project.h"
#include <vector>
#include <optional>
#include <memory>
#include <spdlog/spdlog.h>

// Conditional includes based on available MySQL API
#ifdef USE_MYSQL_C_API
    #include <mysql/mysql.h>
#else
    #include <mysqlx/xdevapi.h>
#endif

namespace aeronautical {

struct ProjectFilter {
    std::optional<ProjectStatus> status;
    std::optional<int> demander_id;
    std::optional<ProjectPriority> priority;
    int limit = 100;
    int offset = 0;
};

class ProjectRepository {
public:
    ProjectRepository();
    ~ProjectRepository() = default;
    
    // CRUD operations
    std::vector<Project> findAll(const ProjectFilter& filter = {});
    std::optional<Project> findById(int id);
    std::optional<Project> findByCode(const std::string& code);
    Project create(const Project& project);
    bool update(int id, const Project& project);
    bool deleteById(int id);
        std::optional<std::string> findGeometriesByProjectId(int project_id);

    
    // Statistics
    int count(const ProjectFilter& filter = {});
    
private:
    std::shared_ptr<spdlog::logger> logger_;
    
#ifdef USE_MYSQL_C_API
    Project rowToProject(MYSQL_ROW row, unsigned long* lengths);
    std::string buildSelectQuery() const;
    std::string buildCountQuery() const;
#else
    Project rowToProject(mysqlx::Row& row);
    void setProjectParams(mysqlx::SqlStatement& stmt, const Project& project, int startIndex = 1);
#endif
};

} // namespace aeronautical
