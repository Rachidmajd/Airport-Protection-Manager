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
            return (OGRGeometryH)geometry;
        } else {
            // Try to parse it as a direct geometry
            OGRGeometry* geometry = OGRGeometryFactory::createFromGeoJson(geojson.c_str());
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

    int conflicts_found = 0;
    // 4. Loop through each protection zone
    for (const auto& protection : all_protections) {
        OGRGeometryH hProtGeom = createGeometryFromGeoJSON(protection.protection_geometry);
        if (!hProtGeom) {
            spdlog::warn("Could not parse protection geometry for procedure {}, skipping \n\n protection code \n {}", protection.procedure_id, protection.protection_geometry);
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