#include "ConflictController.h"
#include <spdlog/spdlog.h>
#include "gdal.h"
#include "ogr_api.h"
#include "ogr_geometry.h"
#include "ogr_spatialref.h"
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

// Helper function to create geometry from GeoJSON compatible with GDAL 3.0.4
OGRGeometryH createGeometryFromGeoJSON(const std::string& geojson) {
    // For GDAL 3.0.4, use the C++ API which is more reliable
    OGRGeometry* geometry = nullptr;
    
    // Method 1: Use OGRGeometryFactory::createFromGeoJson (GDAL 3.0+)
    geometry = OGRGeometryFactory::createFromGeoJson(geojson.c_str());
    if (geometry != nullptr) {
        return (OGRGeometryH)geometry;
    }
    
    // Method 2: Try using OGR_G_CreateGeometryFromJson (if available)
    OGRGeometryH hGeom = OGR_G_CreateGeometryFromJson(geojson.c_str());
    if (hGeom) {
        return hGeom;
    }
    
    // Method 3: Log error and return null
    spdlog::error("Failed to create geometry from GeoJSON: {}", geojson.substr(0, 200));
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

    int conflicts_found = 0;
    // 4. Loop through each protection zone
    for (const auto& protection : all_protections) {
        OGRGeometryH hProtGeom = createGeometryFromGeoJSON(protection.protection_geometry);
        if (!hProtGeom) {
            spdlog::warn("Could not parse protection geometry for procedure {}, skipping", protection.procedure_id);
            continue; // Skip invalid protection geometries
        }
        
        OGRGeometry *protection_geometry = (OGRGeometry *)hProtGeom;

        // 5. THE CORE CHECK: Use GDAL's Intersects() method
        if (project_geometry->Intersects(protection_geometry)) {
            conflicts_found++;
            
            OGRGeometryH hIntersection = OGR_G_Intersection(hProjGeom, hProtGeom);
            if (hIntersection) {
                char* intersection_json = nullptr;
                
                // For GDAL 3.0.4, use OGR_G_ExportToJson
                intersection_json = OGR_G_ExportToJson(hIntersection);
                
                std::string description = "Conflict with procedure " + std::to_string(protection.procedure_id) 
                                        + " in protection area '" + protection.protection_name + "'.";

                repository_->create(project_id, protection.procedure_id, description, 
                                  intersection_json ? intersection_json : "{}");

                if (intersection_json) {
                    CPLFree(intersection_json);
                }
                OGR_G_DestroyGeometry(hIntersection); // Use C API for cleanup
            }
        }
        
        OGR_G_DestroyGeometry(hProtGeom); // Use C API for cleanup
    }

    spdlog::info("C++ conflict analysis for project {} complete. Found {} conflicts.", project_id, conflicts_found);
    OGR_G_DestroyGeometry(hProjGeom); // Use C API for cleanup
}

} // namespace aeronautical