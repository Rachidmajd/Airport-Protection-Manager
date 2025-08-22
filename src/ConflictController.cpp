#include "ConflictController.h"
#include <spdlog/spdlog.h>
#include "gdal.h"
#include "ogr_api.h"
#include "ogr_geometry.h"
#include "ogr_spatialref.h"
#include "ProjectRepository.h"
#include <mutex>
#include <memory>
#include <json.hpp>

namespace aeronautical {

// Static member definitions for singleton pattern
std::unique_ptr<ConflictController> ConflictController::instance_ = nullptr;
std::once_flag ConflictController::once_flag_;

ConflictController::ConflictController() 
    : repository_(std::make_unique<ConflictRepository>()) {
    GDALAllRegister();
}

// Singleton getInstance method
ConflictController& ConflictController::getInstance() {
    std::call_once(once_flag_, []() {
        instance_ = std::unique_ptr<ConflictController>(new ConflictController());
    });
    return *instance_;
}

void ConflictController::registerRoutes(crow::SimpleApp& app) {
    auto logger_ = spdlog::get("aeronautical");

    // GET /api/projects/:id/conflicts
    CROW_ROUTE(app, "/api/projects/<int>/conflicts")
        .methods(crow::HTTPMethod::GET)
        ([this](int project_id) {
            return getConflictsByProject(project_id);
        });

    logger_->info("Conflict routes registered");
}

crow::response ConflictController::getConflictsByProject(int project_id) {
    auto logger_ = spdlog::get("aeronautical");
    try {
        
        auto conflicts = repository_->findByProjectId(project_id);
        
        nlohmann::json response_data = nlohmann::json::array();
        for (const auto& conflict : conflicts) {
            response_data.push_back(conflict.toJson());
        }
        
        return crow::response(200, response_data.dump());

    } catch (const std::exception& e) {
        logger_->error("Failed to get conflicts for project {}: {}", project_id, e.what());
        return crow::response(500, "{\"error\":\"Internal server error\"}");
    }
}

// Helper function to create geometry from GeoJSON - FIXED to handle FeatureCollections
OGRGeometryH createGeometryFromGeoJSON(const std::string& geojson) {
    try {
        // Parse the JSON to check if it's a FeatureCollection
        nlohmann::json j = nlohmann::json::parse(geojson);
        
        if (j.contains("type") && j["type"] == "FeatureCollection") {
            // It's a FeatureCollection - we need to extract and merge geometries
            if (!j.contains("features") || !j["features"].is_array() || j["features"].empty()) {
                spdlog::error("FeatureCollection has no features");
                return nullptr;
            }
            
            OGRGeometryCollection* collection = new OGRGeometryCollection();
            
            for (const auto& feature : j["features"]) {
                if (!feature.contains("geometry")) continue;
                
                std::string geom_str = feature["geometry"].dump();
                OGRGeometry* geom = OGRGeometryFactory::createFromGeoJson(geom_str.c_str());
                
                if (geom) {
                    // Fix invalid geometries before adding
                    if (!geom->IsValid()) {
                        spdlog::warn("Fixing invalid geometry in FeatureCollection");
                        OGRGeometry* fixed = geom->Buffer(0); // Buffer(0) can fix self-intersections
                        if (fixed) {
                            delete geom;
                            geom = fixed;
                        }
                    }
                    collection->addGeometry(geom);
                    delete geom; // Collection takes ownership, but we need to clean up our pointer
                }
            }
            
            // If we only have one geometry, return it directly
            // Otherwise, return a union of all geometries
            if (collection->getNumGeometries() == 1) {
                OGRGeometry* single = collection->getGeometryRef(0)->clone();
                delete collection;
                return (OGRGeometryH)single;
            } else if (collection->getNumGeometries() > 1) {
                // Create a union of all geometries
                OGRGeometry* unionGeom = collection->UnionCascaded();
                delete collection;
                return (OGRGeometryH)unionGeom;
            } else {
                delete collection;
                return nullptr;
            }
        } else if (j.contains("type") && j["type"] == "Feature") {
            // It's a single Feature - extract the geometry
            if (!j.contains("geometry")) {
                spdlog::error("Feature has no geometry");
                return nullptr;
            }
            
            std::string geom_str = j["geometry"].dump();
            OGRGeometry* geometry = OGRGeometryFactory::createFromGeoJson(geom_str.c_str());
            
            // Fix invalid geometry
            if (geometry && !geometry->IsValid()) {
                spdlog::warn("Fixing invalid Feature geometry");
                OGRGeometry* fixed = geometry->Buffer(0);
                if (fixed) {
                    delete geometry;
                    geometry = fixed;
                }
            }
            
            return (OGRGeometryH)geometry;
        } else {
            // Try to parse it as a direct geometry
            OGRGeometry* geometry = OGRGeometryFactory::createFromGeoJson(geojson.c_str());
            
            // Fix invalid geometry
            if (geometry && !geometry->IsValid()) {
                spdlog::warn("Fixing invalid direct geometry");
                OGRGeometry* fixed = geometry->Buffer(0);
                if (fixed) {
                    delete geometry;
                    geometry = fixed;
                }
            }
            
            if (geometry) {
                return (OGRGeometryH)geometry;
            }
            
            // Fallback to C API
            OGRGeometryH hGeom = OGR_G_CreateGeometryFromJson(geojson.c_str());
            if (hGeom) {
                return hGeom;
            }
        }
    } catch (const std::exception& e) {
        spdlog::error("Exception while parsing GeoJSON: {}", e.what());
    }
    
    spdlog::error("Failed to create geometry from GeoJSON");
    return nullptr;
}

void ConflictController::analyzeProject(int project_id) {
    spdlog::info("Starting C++ conflict analysis for project ID: {}", project_id);

    // 1. Setup
    repository_->deleteByProjectId(project_id);
    ProjectRepository proj_repo;
    FlightProcedureRepository proc_repo;

    // 2. Fetch Geometries
    auto project_geom_json = proj_repo.findGeometriesByProjectId(project_id);
    auto all_protections = proc_repo.findAllActiveProtections();

    if (!project_geom_json || all_protections.empty()) {
        spdlog::warn("No project geometry or no protection zones found. Aborting analysis for project {}.", project_id);
        return;
    }

    // 3. Create GDAL Geometry for the project using the helper function
    OGRGeometryH hProjGeom = createGeometryFromGeoJSON(*project_geom_json);
    if (!hProjGeom) {
        spdlog::error("Could not parse project geometry for project {}.", project_id);
        return;
    }
    
    // Cast the C handle to a C++ object pointer
    OGRGeometry *project_geometry = (OGRGeometry *)hProjGeom;
    
    // Validate and fix project geometry if needed
    if (!project_geometry->IsValid()) {
        spdlog::warn("Project geometry is invalid, attempting to fix");
        OGRGeometry* fixed = project_geometry->Buffer(0);
        if (fixed) {
            OGR_G_DestroyGeometry(hProjGeom);
            hProjGeom = (OGRGeometryH)fixed;
            project_geometry = fixed;
        }
    }

    int conflicts_found = 0;
    // 4. Loop through each protection zone
    for (const auto& protection : all_protections) {
        OGRGeometryH hProtGeom = createGeometryFromGeoJSON(protection.protection_geometry);
        if (!hProtGeom) {
            spdlog::warn("Could not parse protection geometry for procedure {}, skipping", protection.procedure_id);
            continue; // Skip invalid protection geometries
        }
        
        OGRGeometry *protection_geometry = (OGRGeometry *)hProtGeom;
        
        // Validate and fix protection geometry if needed
        if (!protection_geometry->IsValid()) {
            spdlog::warn("Protection geometry for procedure {} is invalid, attempting to fix", protection.procedure_id);
            OGRGeometry* fixed = protection_geometry->Buffer(0);
            if (fixed) {
                OGR_G_DestroyGeometry(hProtGeom);
                hProtGeom = (OGRGeometryH)fixed;
                protection_geometry = fixed;
            }
        }

        // 5. THE CORE CHECK: Use GDAL's Intersects() method
        bool intersects = false;
        try {
            intersects = project_geometry->Intersects(protection_geometry);
        } catch (const std::exception& e) {
            spdlog::error("Exception during intersection check: {}", e.what());
            OGR_G_DestroyGeometry(hProtGeom);
            continue;
        }
        
        if (intersects) {
            conflicts_found++;
            
            // Try to compute intersection, but save conflict even if intersection fails
            OGRGeometryH hIntersection = nullptr;
            std::string intersection_json = "{}";
            
            try {
                hIntersection = OGR_G_Intersection(hProjGeom, hProtGeom);
                if (hIntersection) {
                    char* json_str = OGR_G_ExportToJson(hIntersection);
                    if (json_str) {
                        intersection_json = json_str;
                        CPLFree(json_str);
                    }
                    OGR_G_DestroyGeometry(hIntersection);
                }
            } catch (const std::exception& e) {
                spdlog::warn("Failed to compute intersection geometry: {}", e.what());
                // Continue anyway - we still want to record the conflict
            }
            
            // Even if we couldn't compute the exact intersection, 
            // we still know there's a conflict, so save it
            std::string description = "Conflict with procedure " + std::to_string(protection.procedure_id) 
                                    + " in protection area '" + protection.protection_name + "'.";
            
            // Save the conflict (with or without intersection geometry)
            if (!repository_->create(project_id, protection.procedure_id, description, intersection_json)) {
                spdlog::error("Failed to save conflict to database for project {} and procedure {}", 
                             project_id, protection.procedure_id);
            } else {
                spdlog::info("Saved conflict for project {} with procedure {}", 
                            project_id, protection.procedure_id);
            }
        }
        
        OGR_G_DestroyGeometry(hProtGeom); // Use C API for cleanup
    }

    spdlog::info("C++ conflict analysis for project {} complete. Found {} conflicts.", project_id, conflicts_found);
    OGR_G_DestroyGeometry(hProjGeom); // Use C API for cleanup

    //updating the project status
    {
        auto projectToUpdateOpt = proj_repo.findById(project_id);
        if (projectToUpdateOpt) {
            Project projectToUpdate = *projectToUpdateOpt;
            
            // 2. Update the status to UnderReview
            projectToUpdate.status = ProjectStatus::UnderReview;
            
            // 3. Save the updated project back to the database
            if (proj_repo.update(project_id, projectToUpdate)) {
                spdlog::info("Successfully updated project {} status to UnderReview.", project_id);
            } else {
                spdlog::error("Failed to update project {} status after analysis.", project_id);
            }
        } else {
            spdlog::error("Could not find project {} to update its status after analysis.", project_id);
        }
    }
}

} // namespace aeronautical