#pragma once

#include "OLSSurfaceGenerator.h"
#include "Airport.h"
#include "FlightProcedure.h"
#include <json.hpp>
#include <vector>

namespace aeronautical {

/**
 * Results of an OLS penetration analysis
 */
struct OLSPenetrationResult {
    int obstacle_id;
    std::string obstacle_name;
    GeoPoint3D location;
    double penetration_m;           // Positive = penetrates, negative = clearance
    std::string penetrated_surface; // Which OLS surface was penetrated
    std::string runway_designator;
    std::string airport_icao;
    bool is_critical;               // Penetration > threshold
    
    nlohmann::json toJson() const {
        nlohmann::json j;
        j["obstacle_id"] = obstacle_id;
        j["obstacle_name"] = obstacle_name;
        j["location"] = {
            {"latitude", location.latitude},
            {"longitude", location.longitude},
            {"elevation_m", location.elevation}
        };
        j["penetration_m"] = penetration_m;
        j["penetrated_surface"] = penetrated_surface;
        j["runway_designator"] = runway_designator;
        j["airport_icao"] = airport_icao;
        j["is_critical"] = is_critical;
        j["clearance_m"] = penetration_m < 0 ? std::abs(penetration_m) : 0;
        return j;
    }
};

/**
 * OLS Analysis Integration
 * 
 * Integrates ICAO Annex 14 OLS analysis with the existing conflict detection system.
 * Can be used to:
 *   1. Check project geometries against all OLS at nearby airports
 *   2. Generate OLS for flight procedures
 *   3. Combine OLS with flight procedure protections
 */
class OLSAnalysisIntegration {
public:
    OLSAnalysisIntegration();
    ~OLSAnalysisIntegration() = default;
    
    /**
     * Analyze a project's obstacles against OLS at nearby airports
     * 
     * @param project_geometry_json GeoJSON FeatureCollection of project obstacles
     * @param nearby_airports Airports within analysis range
     * @param critical_threshold Penetration threshold for critical flagging (m)
     * @return Vector of penetration results
     */
    std::vector<OLSPenetrationResult> analyzeProjectAgainstOLS(
        const std::string& project_geometry_json,
        const std::vector<Airport>& nearby_airports,
        double critical_threshold = 0.0
    );
    
    /**
     * Generate combined OLS and flight procedure protection GeoJSON
     * 
     * @param airport_icao Airport to analyze
     * @param runway_data Runway information
     * @param procedure Optional flight procedure to include
     * @return Combined GeoJSON FeatureCollection
     */
    nlohmann::json generateCombinedProtectionZones(
        const std::string& airport_icao,
        const AirportRunway& runway_data,
        const std::optional<FlightProcedure>& procedure = std::nullopt
    );
    
    /**
     * Check a single point against OLS
     * 
     * @param point 3D point to check
     * @param airport_icao Airport ICAO code
     * @param runway Runway data
     * @param approach_category Approach category for OLS parameters
     * @return Penetration result
     */
    OLSPenetrationResult checkPointAgainstOLS(
        const GeoPoint3D& point,
        const std::string& airport_icao,
        const AirportRunway& runway,
        ApproachCategory approach_category = ApproachCategory::NonPrecisionApproach
    );
    
    /**
     * Get the most restrictive OLS height at a location
     * 
     * @param location Geographic location
     * @param airport Airport reference
     * @param runway Runway to check against
     * @return Maximum allowed height in meters MSL
     */
    double getMaxAllowedHeight(
        const GeoPoint3D& location,
        const Airport& airport,
        const AirportRunway& runway
    );
    
    /**
     * Create a ProcedureProtection from OLS surface
     * 
     * Useful for integrating OLS into the existing protection zone system
     * 
     * @param surface OLS surface
     * @param procedure_id Associated flight procedure ID (optional)
     * @return ProcedureProtection compatible with existing system
     */
    ProcedureProtection createProtectionFromOLS(
        const OLSSurface& surface,
        int procedure_id = 0
    );
    
private:
    OLSSurfaceGenerator generator_;
    std::shared_ptr<spdlog::logger> logger_;
    
    /**
     * Extract obstacle points from GeoJSON
     */
    std::vector<std::pair<int, GeoPoint3D>> extractObstaclesFromGeoJSON(
        const std::string& geojson
    );
    
    /**
     * Check if a point is within airport analysis range
     */
    bool isWithinAnalysisRange(
        const GeoPoint3D& point,
        const Airport& airport,
        double range_nm = 15.0  // Typical OLS extends ~15nm
    );
    
    /**
     * Convert degrees to nautical miles
     */
    static double degreesToNm(double degrees);
    
    /**
     * Calculate distance between two points in nautical miles
     */
    static double distanceNm(const GeoPoint3D& p1, const GeoPoint3D& p2);
};

// ============================================================================
// Inline Utility Functions
// ============================================================================

inline double OLSAnalysisIntegration::degreesToNm(double degrees) {
    return degrees * 60.0;  // 1 degree latitude ≈ 60 NM
}

inline double OLSAnalysisIntegration::distanceNm(const GeoPoint3D& p1, const GeoPoint3D& p2) {
    double lat_diff = std::abs(p1.latitude - p2.latitude);
    double lon_diff = std::abs(p1.longitude - p2.longitude);
    
    // Approximate using equirectangular projection
    double avg_lat = (p1.latitude + p2.latitude) / 2.0;
    double lat_rad = avg_lat * 3.14159265 / 180.0;
    
    double x = lon_diff * std::cos(lat_rad);
    double y = lat_diff;
    
    return std::sqrt(x * x + y * y) * 60.0;  // Convert to NM
}

} // namespace aeronautical
