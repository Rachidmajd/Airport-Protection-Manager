#include "ConflictController.h"
#include <spdlog/spdlog.h>
#include "gdal.h"      // Use the C-style header
#include "ogr_api.h"   // for OGR_G_... functions
#include "ogr_geometry.h" // for OGRGeometry class

namespace aeronautical {

ConflictController::ConflictController() 
    : repository_(std::make_unique<ConflictRepository>()) {
    GDALAllRegister();
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

    // 3. Create GDAL Geometry for the project using the C API
    OGRGeometryH hProjGeom = OGR_G_CreateFromGeoJson(project_geom_json->c_str());
    if (!hProjGeom) {
        spdlog::error("Could not parse project geometry for project {}.", project_id);
        return;
    }
    // Cast the C handle to a C++ object pointer
    OGRGeometry *project_geometry = (OGRGeometry *)hProjGeom;

    int conflicts_found = 0;
    // 4. Loop through each protection zone
    for (const auto& protection : all_protections) {
        OGRGeometryH hProtGeom = OGR_G_CreateFromGeoJson(protection.protection_geometry.c_str());
        if (!hProtGeom) continue; // Skip invalid protection geometries
        
        OGRGeometry *protection_geometry = (OGRGeometry *)hProtGeom;

        // 5. THE CORE CHECK: Use GDAL's Intersects() method
        if (project_geometry->Intersects(protection_geometry)) {
            conflicts_found++;
            
            OGRGeometryH hIntersection = OGR_G_Intersection(hProjGeom, hProtGeom);
            if (hIntersection) {
                char* intersection_json = OGR_G_ExportToJson(hIntersection);
                
                std::string description = "Conflict with procedure " + std::to_string(protection.procedure_id) 
                                        + " in protection area '" + protection.protection_name + "'.";

                repository_->create(project_id, protection.procedure_id, description, intersection_json);

                CPLFree(intersection_json);
                OGR_G_DestroyGeometry(hIntersection); // Use C API for cleanup
            }
        }
        
        OGR_G_DestroyGeometry(hProtGeom); // Use C API for cleanup
    }

    spdlog::info("C++ conflict analysis for project {} complete. Found {} conflicts.", project_id, conflicts_found);
    OGR_G_DestroyGeometry(hProjGeom); // Use C API for cleanup
}

} // namespace aeronautical