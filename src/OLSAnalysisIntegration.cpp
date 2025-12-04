#include "OLSAnalysisIntegration.h"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace aeronautical {

OLSAnalysisIntegration::OLSAnalysisIntegration() {
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

std::vector<std::pair<int, GeoPoint3D>> OLSAnalysisIntegration::extractObstaclesFromGeoJSON(
    const std::string& geojson) {
    
    std::vector<std::pair<int, GeoPoint3D>> obstacles;
    
    try {
        nlohmann::json j = nlohmann::json::parse(geojson);
        
        auto processFeature = [&](const nlohmann::json& feature, int index) {
            if (!feature.contains("geometry")) return;
            
            const auto& geometry = feature["geometry"];
            std::string type = geometry["type"];
            
            GeoPoint3D point;
            
            if (type == "Point") {
                const auto& coords = geometry["coordinates"];
                point.longitude = coords[0].get<double>();
                point.latitude = coords[1].get<double>();
                point.elevation = coords.size() > 2 ? coords[2].get<double>() : 0;
                
                // Get elevation from properties if not in coordinates
                if (point.elevation == 0 && feature.contains("properties")) {
                    const auto& props = feature["properties"];
                    if (props.contains("elevation_m")) {
                        point.elevation = props["elevation_m"].get<double>();
                    } else if (props.contains("altitude_max")) {
                        point.elevation = props["altitude_max"].get<double>() * 0.3048;  // ft to m
                    } else if (props.contains("height_m")) {
                        point.elevation = props["height_m"].get<double>();
                    }
                }
                
                obstacles.push_back({index, point});
                
            } else if (type == "Polygon" || type == "MultiPolygon") {
                // For polygons, extract the highest point or centroid with max elevation
                double max_elevation = 0;
                double sum_lat = 0, sum_lon = 0;
                int count = 0;
                
                // Get elevation from properties
                if (feature.contains("properties")) {
                    const auto& props = feature["properties"];
                    if (props.contains("elevation_m")) {
                        max_elevation = props["elevation_m"].get<double>();
                    } else if (props.contains("altitude_max")) {
                        max_elevation = props["altitude_max"].get<double>() * 0.3048;
                    }
                }
                
                // Calculate centroid
                const auto& coords = geometry["coordinates"];
                if (type == "Polygon" && coords.size() > 0) {
                    for (const auto& coord : coords[0]) {
                        sum_lon += coord[0].get<double>();
                        sum_lat += coord[1].get<double>();
                        if (coord.size() > 2) {
                            double z = coord[2].get<double>();
                            if (z > max_elevation) max_elevation = z;
                        }
                        count++;
                    }
                } else if (type == "MultiPolygon" && coords.size() > 0) {
                    for (const auto& polygon : coords) {
                        if (polygon.size() > 0) {
                            for (const auto& coord : polygon[0]) {
                                sum_lon += coord[0].get<double>();
                                sum_lat += coord[1].get<double>();
                                if (coord.size() > 2) {
                                    double z = coord[2].get<double>();
                                    if (z > max_elevation) max_elevation = z;
                                }
                                count++;
                            }
                        }
                    }
                }
                
                if (count > 0) {
                    point.latitude = sum_lat / count;
                    point.longitude = sum_lon / count;
                    point.elevation = max_elevation;
                    obstacles.push_back({index, point});
                }
            }
        };
        
        if (j.contains("type") && j["type"] == "FeatureCollection") {
            int index = 0;
            for (const auto& feature : j["features"]) {
                processFeature(feature, index++);
            }
        } else if (j.contains("type") && j["type"] == "Feature") {
            processFeature(j, 0);
        }
        
    } catch (const std::exception& e) {
        logger_->error("Error extracting obstacles from GeoJSON: {}", e.what());
    }
    
    return obstacles;
}

bool OLSAnalysisIntegration::isWithinAnalysisRange(
    const GeoPoint3D& point,
    const Airport& airport,
    double range_nm) {
    
    GeoPoint3D airport_point(airport.latitude, airport.longitude, 0);
    double distance = distanceNm(point, airport_point);
    return distance <= range_nm;
}

std::vector<OLSPenetrationResult> OLSAnalysisIntegration::analyzeProjectAgainstOLS(
    const std::string& project_geometry_json,
    const std::vector<Airport>& nearby_airports,
    double critical_threshold) {
    
    std::vector<OLSPenetrationResult> results;
    
    // Extract obstacles from project geometry
    auto obstacles = extractObstaclesFromGeoJSON(project_geometry_json);
    
    if (obstacles.empty()) {
        logger_->warn("No obstacles extracted from project geometry");
        return results;
    }
    
    logger_->info("Analyzing {} obstacles against OLS at {} airports",
                 obstacles.size(), nearby_airports.size());
    
    for (const auto& airport : nearby_airports) {
        // Skip airports without runway data
        if (airport.runway_count == 0) continue;
        
        // Create runway end data from airport info
        // In production, you'd fetch actual runway data
        RunwayEndData runway_end;
        runway_end.designator = "00";  // Placeholder
        runway_end.threshold_lat = airport.latitude;
        runway_end.threshold_lon = airport.longitude;
        runway_end.threshold_elevation_m = airport.elevation_ft * 0.3048;
        runway_end.runway_length_m = airport.longest_runway_ft * 0.3048;
        runway_end.runway_width_m = 45;  // Default
        runway_end.code_number = OLSSurfaceGenerator::determineCodeNumber(runway_end.runway_length_m);
        runway_end.code_letter = OLSSurfaceGenerator::determineCodeLetter(runway_end.runway_width_m);
        runway_end.approach_category = airport.has_ils ? 
            ApproachCategory::PrecisionCategoryI : ApproachCategory::NonPrecisionApproach;
        runway_end.heading_deg = 0;  // Would need actual heading
        
        // Generate OLS surfaces
        RunwayOLSSurfaces surfaces = generator_.generateSurfaces(runway_end);
        surfaces.airport_icao = airport.icao_code;
        
        // Check each obstacle
        for (const auto& [obs_id, obs_point] : obstacles) {
            // Skip if outside analysis range
            if (!isWithinAnalysisRange(obs_point, airport, 15.0)) {
                continue;
            }
            
            double penetration = generator_.checkPenetration(obs_point, surfaces);
            
            if (penetration > -100) {  // Only report if reasonably close to surface
                OLSPenetrationResult result;
                result.obstacle_id = obs_id;
                result.obstacle_name = "Obstacle " + std::to_string(obs_id);
                result.location = obs_point;
                result.penetration_m = penetration;
                result.runway_designator = runway_end.designator;
                result.airport_icao = airport.icao_code;
                result.is_critical = penetration > critical_threshold;
                
                // Determine which surface was penetrated (simplified)
                if (penetration > 0) {
                    result.penetrated_surface = "OLS Surface";  // Would need more specific check
                } else {
                    result.penetrated_surface = "None (clearance)";
                }
                
                results.push_back(result);
            }
        }
    }
    
    // Sort by penetration (most critical first)
    std::sort(results.begin(), results.end(),
              [](const OLSPenetrationResult& a, const OLSPenetrationResult& b) {
                  return a.penetration_m > b.penetration_m;
              });
    
    logger_->info("OLS analysis complete: {} results, {} penetrations",
                 results.size(),
                 std::count_if(results.begin(), results.end(),
                              [](const OLSPenetrationResult& r) { return r.penetration_m > 0; }));
    
    return results;
}

nlohmann::json OLSAnalysisIntegration::generateCombinedProtectionZones(
    const std::string& airport_icao,
    const AirportRunway& runway_data,
    const std::optional<FlightProcedure>& procedure) {
    
    nlohmann::json feature_collection;
    feature_collection["type"] = "FeatureCollection";
    feature_collection["features"] = nlohmann::json::array();
    
    // Generate OLS surfaces for both runway ends
    auto ols_surfaces = generator_.generateSurfacesForRunway(
        runway_data,
        0,  // Airport elevation - should be passed in
        ApproachCategory::NonPrecisionApproach,
        ApproachCategory::NonPrecisionApproach
    );
    
    // Add OLS features
    for (const auto& surface_set : ols_surfaces) {
        auto ols_geojson = surface_set.toGeoJSON();
        for (const auto& feature : ols_geojson["features"]) {
            nlohmann::json f = feature;
            f["properties"]["source"] = "annex14_ols";
            feature_collection["features"].push_back(f);
        }
    }
    
    // Add flight procedure protection if provided
    if (procedure && procedure->protection_geometry) {
        try {
            nlohmann::json proc_geom = nlohmann::json::parse(*procedure->protection_geometry);
            
            if (proc_geom.contains("type") && proc_geom["type"] == "FeatureCollection") {
                for (const auto& feature : proc_geom["features"]) {
                    nlohmann::json f = feature;
                    f["properties"]["source"] = "flight_procedure";
                    f["properties"]["procedure_code"] = procedure->procedure_code;
                    feature_collection["features"].push_back(f);
                }
            } else {
                nlohmann::json f;
                f["type"] = "Feature";
                f["geometry"] = proc_geom;
                f["properties"] = {
                    {"source", "flight_procedure"},
                    {"procedure_code", procedure->procedure_code}
                };
                feature_collection["features"].push_back(f);
            }
        } catch (const std::exception& e) {
            logger_->warn("Error parsing procedure protection geometry: {}", e.what());
        }
    }
    
    feature_collection["properties"] = {
        {"airport_icao", airport_icao},
        {"runway_identifier", runway_data.runway_identifier},
        {"generated_at", std::time(nullptr)}
    };
    
    return feature_collection;
}

OLSPenetrationResult OLSAnalysisIntegration::checkPointAgainstOLS(
    const GeoPoint3D& point,
    const std::string& airport_icao,
    const AirportRunway& runway,
    ApproachCategory approach_category) {
    
    OLSPenetrationResult result;
    result.location = point;
    result.airport_icao = airport_icao;
    
    // Create runway end data
    RunwayEndData runway_end;
    runway_end.designator = runway.le_ident;
    runway_end.threshold_lat = runway.le_latitude;
    runway_end.threshold_lon = runway.le_longitude;
    runway_end.threshold_elevation_m = 0;  // Would need from airport
    runway_end.heading_deg = runway.le_heading_deg;
    runway_end.runway_length_m = runway.length_ft * 0.3048;
    runway_end.runway_width_m = runway.width_ft * 0.3048;
    runway_end.code_number = OLSSurfaceGenerator::determineCodeNumber(runway_end.runway_length_m);
    runway_end.code_letter = OLSSurfaceGenerator::determineCodeLetter(runway_end.runway_width_m);
    runway_end.approach_category = approach_category;
    
    // Generate surfaces and check
    RunwayOLSSurfaces surfaces = generator_.generateSurfaces(runway_end);
    
    result.penetration_m = generator_.checkPenetration(point, surfaces);
    result.runway_designator = runway_end.designator;
    result.is_critical = result.penetration_m > 0;
    
    if (result.penetration_m > 0) {
        result.penetrated_surface = "OLS";
    } else {
        result.penetrated_surface = "None";
    }
    
    return result;
}

double OLSAnalysisIntegration::getMaxAllowedHeight(
    const GeoPoint3D& location,
    const Airport& airport,
    const AirportRunway& runway) {
    
    // Create a test point at very high elevation
    GeoPoint3D test_point = location;
    test_point.elevation = 10000;  // 10km - above any OLS
    
    // Check against OLS
    OLSPenetrationResult result = checkPointAgainstOLS(
        test_point, airport.icao_code, runway, ApproachCategory::NonPrecisionApproach);
    
    // Calculate max allowed height
    // If penetration is X meters, then max height is test_elevation - X
    double max_height = test_point.elevation - result.penetration_m;
    
    // Ensure it's reasonable (not below ground)
    double ground_level = airport.elevation_ft * 0.3048;
    if (max_height < ground_level) {
        max_height = ground_level;
    }
    
    return max_height;
}

ProcedureProtection OLSAnalysisIntegration::createProtectionFromOLS(
    const OLSSurface& surface,
    int procedure_id) {
    
    ProcedureProtection protection;
    
    protection.id = 0;  // Will be assigned by database
    protection.procedure_id = procedure_id;
    protection.protection_name = surface.name;
    
    // Map OLS surface type to protection type
    if (surface.surface_type == "approach") {
        protection.protection_type = ProtectionType::ObstacleClearance;
    } else if (surface.surface_type == "takeoff_climb") {
        protection.protection_type = ProtectionType::ObstacleClearance;
    } else if (surface.surface_type == "transitional") {
        protection.protection_type = ProtectionType::BufferZone;
    } else if (surface.surface_type == "inner_horizontal") {
        protection.protection_type = ProtectionType::ObstacleClearance;
    } else if (surface.surface_type == "conical") {
        protection.protection_type = ProtectionType::ObstacleClearance;
    } else if (surface.surface_type == "outer_horizontal") {
        protection.protection_type = ProtectionType::ObstacleClearance;
    } else {
        protection.protection_type = ProtectionType::OverallPrimary;
    }
    
    protection.description = "ICAO Annex 14 " + surface.name;
    
    // Convert surface geometry to GeoJSON string
    protection.protection_geometry = surface.toGeoJSON()["geometry"].dump();
    
    // Set altitude limits
    if (!surface.boundary_points.empty()) {
        double min_elev = surface.boundary_points[0].elevation;
        double max_elev = surface.boundary_points[0].elevation;
        
        for (const auto& pt : surface.boundary_points) {
            if (pt.elevation < min_elev) min_elev = pt.elevation;
            if (pt.elevation > max_elev) max_elev = pt.elevation;
        }
        
        protection.altitude_min = static_cast<int>(min_elev * 3.28084);  // m to ft
        protection.altitude_max = static_cast<int>(max_elev * 3.28084);
    }
    
    protection.altitude_reference = AltitudeReference::MSL;
    protection.restriction_level = RestrictionLevel::Prohibited;
    protection.conflict_severity = ConflictSeverity::Critical;
    protection.analysis_priority = 100;  // Highest priority for OLS
    protection.regulatory_source = "ICAO Annex 14";
    protection.is_active = true;
    protection.created_at = std::chrono::system_clock::now();
    protection.updated_at = std::chrono::system_clock::now();
    
    return protection;
}

} // namespace aeronautical
