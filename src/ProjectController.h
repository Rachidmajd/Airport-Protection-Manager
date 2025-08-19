#pragma once

#include <crow.h>
#include "ProjectRepository.h"
#include "ConflictController.h" 
#include <memory>
#include <spdlog/spdlog.h>

namespace aeronautical {

class ProjectController {
public:
    ProjectController();
    ~ProjectController() = default;
    
    void registerRoutes(crow::SimpleApp& app);
    
private:
    std::unique_ptr<ProjectRepository> repository_;
    std::unique_ptr<ConflictController> conflict_controller_; 
    std::shared_ptr<spdlog::logger> logger_;
    
    // Route handlers
    crow::response getProjects(const crow::request& req);
    crow::response getProject(int id);
    crow::response getProjectByCode(const std::string& code);
    crow::response createProject(const crow::request& req);
    crow::response updateProject(int id, const crow::request& req);
    crow::response deleteProject(int id);
    crow::response submitProject(int id, const crow::request& req);
    bool validateGeoJSON(const nlohmann::json& geojson, std::string& error);
    bool validateGeometry(const nlohmann::json& geometry, std::string& error);
    bool saveProjectGeometry(int project_id, const nlohmann::json& geojson);
    void addProjectComment(int project_id, const std::string& comment, 
                          ProjectStatus oldStatus, ProjectStatus newStatus);
    bool saveIndividualGeometry(int project_id, const nlohmann::json& feature, 
                               int defaultAltMin, int defaultAltMax, bool isPrimary);
    double calculatePolygonArea(const nlohmann::json& coordinates);
    bool saveOrUpdateProjectGeometryCollection(int project_id, const nlohmann::json& incoming_geojson);

    crow::response getProjectGeometries(int project_id);


    
    // Helper methods
    crow::response errorResponse(int code, const std::string& message);
    crow::response successResponse(const nlohmann::json& data);
    bool validateProjectInput(const nlohmann::json& input, std::string& error);
    bool checkAuthorization(const crow::request& req, std::string& error);
};

} // namespace aeronautical
