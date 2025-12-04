#pragma once

#include <string>
#include <vector>
#include <optional>
#include <cmath>
#include <json.hpp>
#include "Airport.h"
#include <spdlog/spdlog.h>


namespace aeronautical {

// ============================================================================
// ICAO Annex 14 Runway Classification
// ============================================================================

/**
 * Runway Code Number (based on Reference Field Length)
 * Code 1: < 800m
 * Code 2: 800m to < 1200m
 * Code 3: 1200m to < 1800m
 * Code 4: >= 1800m
 */
enum class RunwayCodeNumber {
    Code1 = 1,
    Code2 = 2,
    Code3 = 3,
    Code4 = 4
};

/**
 * Runway Code Letter (based on wingspan and outer main gear wheel span)
 * A: wingspan < 15m
 * B: 15m <= wingspan < 24m
 * C: 24m <= wingspan < 36m
 * D: 36m <= wingspan < 52m
 * E: 52m <= wingspan < 65m
 * F: 65m <= wingspan < 80m
 */
enum class RunwayCodeLetter {
    A, B, C, D, E, F
};

/**
 * Approach Category for OLS determination
 */
enum class ApproachCategory {
    NonInstrument,          // Visual approaches only
    NonPrecisionApproach,   // NPA (VOR, NDB, LNAV, etc.)
    PrecisionCategoryI,     // ILS CAT I
    PrecisionCategoryII,    // ILS CAT II
    PrecisionCategoryIII    // ILS CAT III
};

// ============================================================================
// OLS Surface Parameters (from ICAO Annex 14 Tables)
// ============================================================================

/**
 * Parameters for Approach Surface
 */
struct ApproachSurfaceParams {
    double length_inner_edge;      // Width at threshold (m)
    double distance_from_threshold; // Start distance from threshold (m)
    double divergence;             // Each side divergence (% or ratio)
    double first_section_length;   // Length of first (horizontal) section (m)
    double first_section_slope;    // Slope of first section (%)
    double second_section_length;  // Length of second section (m) - precision only
    double second_section_slope;   // Slope of second section (%) - precision only
    double horizontal_section_length; // Length of horizontal section (m) - CAT II/III
    double total_length;           // Total surface length (m)
};

/**
 * Parameters for Take-off Climb Surface
 */
struct TakeoffClimbSurfaceParams {
    double length_inner_edge;      // Width at end of runway (m)
    double distance_from_end;      // Start distance from runway end (m)
    double divergence;             // Each side divergence (%)
    double final_width;            // Maximum width (m)
    double length;                 // Total length (m)
    double slope;                  // Climb slope (%)
};

/**
 * Parameters for Transitional Surface
 */
struct TransitionalSurfaceParams {
    double slope;                  // Slope ratio (e.g., 14.3% = 1:7)
    double height_limit;           // Height above runway elevation (m)
};

/**
 * Parameters for Inner Horizontal Surface
 */
struct InnerHorizontalSurfaceParams {
    double radius;                 // Radius from ARP or runway ends (m)
    double height;                 // Height above aerodrome elevation (m)
};

/**
 * Parameters for Conical Surface
 */
struct ConicalSurfaceParams {
    double slope;                  // Slope (%)
    double height;                 // Vertical extent above inner horizontal (m)
};

/**
 * Parameters for Outer Horizontal Surface (where applicable)
 */
struct OuterHorizontalSurfaceParams {
    double radius;                 // Radius (m)
    double height;                 // Height above aerodrome elevation (m)
};

/**
 * Parameters for Inner Approach Surface (CAT II/III)
 */
struct InnerApproachSurfaceParams {
    double width;                  // Width (m)
    double distance_from_threshold; // Start distance (m)
    double length;                 // Length (m)
    double slope;                  // Slope (%)
};

/**
 * Parameters for Inner Transitional Surface (CAT II/III)
 */
struct InnerTransitionalSurfaceParams {
    double slope;                  // Slope (%)
};

/**
 * Parameters for Balked Landing Surface (CAT II/III)
 */
struct BalkedLandingSurfaceParams {
    double length_inner_edge;      // Width (m)
    double distance_from_threshold; // Start distance (m)
    double divergence;             // Divergence (%)
    double slope;                  // Slope (%)
};

// ============================================================================
// OLS Complete Parameter Set
// ============================================================================

/**
 * Complete set of OLS parameters for a runway end
 */
struct OLSParameters {
    RunwayCodeNumber code_number;
    RunwayCodeLetter code_letter;
    ApproachCategory approach_category;
    
    ApproachSurfaceParams approach;
    TakeoffClimbSurfaceParams takeoff_climb;
    TransitionalSurfaceParams transitional;
    InnerHorizontalSurfaceParams inner_horizontal;
    ConicalSurfaceParams conical;
    
    // Optional surfaces for precision approaches
    std::optional<OuterHorizontalSurfaceParams> outer_horizontal;
    std::optional<InnerApproachSurfaceParams> inner_approach;
    std::optional<InnerTransitionalSurfaceParams> inner_transitional;
    std::optional<BalkedLandingSurfaceParams> balked_landing;
};

// ============================================================================
// Geographic Point with Elevation
// ============================================================================

struct GeoPoint3D {
    double latitude;
    double longitude;
    double elevation;  // meters above MSL
    
    GeoPoint3D() : latitude(0), longitude(0), elevation(0) {}
    GeoPoint3D(double lat, double lon, double elev = 0) 
        : latitude(lat), longitude(lon), elevation(elev) {}
};

// ============================================================================
// OLS Surface Geometry Result
// ============================================================================

/**
 * Represents a single OLS surface with its 3D geometry
 */
struct OLSSurface {
    std::string name;              // Surface name (e.g., "Approach Surface RWY 09")
    std::string surface_type;      // Type identifier
    std::string runway_designator; // Associated runway end
    double base_elevation;         // Reference elevation (m)
    double slope;                  // Surface slope (%)
    std::vector<GeoPoint3D> boundary_points;  // 3D polygon vertices
    nlohmann::json properties;     // Additional properties
    
    /**
     * Convert surface to GeoJSON Feature
     */
    nlohmann::json toGeoJSON() const;
    
    /**
     * Convert surface to GeoJSON with 3D coordinates
     */
    nlohmann::json toGeoJSON3D() const;
};

/**
 * Complete set of OLS surfaces for a runway
 */
struct RunwayOLSSurfaces {
    std::string airport_icao;
    std::string runway_designator;
    double runway_elevation_m;
    
    // Primary surfaces
    OLSSurface approach_surface;
    OLSSurface takeoff_climb_surface;
    OLSSurface transitional_surface;
    OLSSurface inner_horizontal_surface;
    OLSSurface conical_surface;
    
    // Optional precision approach surfaces
    std::optional<OLSSurface> outer_horizontal_surface;
    std::optional<OLSSurface> inner_approach_surface;
    std::optional<OLSSurface> inner_transitional_surface;
    std::optional<OLSSurface> balked_landing_surface;
    
    /**
     * Convert all surfaces to GeoJSON FeatureCollection
     */
    nlohmann::json toGeoJSON() const;
};

// ============================================================================
// Runway End Data for OLS Generation
// ============================================================================

struct RunwayEndData {
    std::string designator;        // e.g., "09", "27L"
    double threshold_lat;
    double threshold_lon;
    double threshold_elevation_m;
    double heading_deg;            // True heading
    double runway_length_m;
    double runway_width_m;
    RunwayCodeNumber code_number;
    RunwayCodeLetter code_letter;
    ApproachCategory approach_category;
    
    // Displaced threshold (if applicable)
    double displaced_threshold_m = 0;
    
    // Strip dimensions
    double strip_length_beyond_end_m = 60;  // Default for Code 3/4
    double strip_width_m = 150;             // Default for Code 3/4 precision
};

// ============================================================================
// OLS Surface Generator Class
// ============================================================================

/**
 * Generates ICAO Annex 14 Obstacle Limitation Surfaces
 * 
 * This class calculates OLS geometries based on runway parameters
 * following ICAO Annex 14 Volume I specifications.
 * 
 * Usage:
 *   OLSSurfaceGenerator generator;
 *   auto surfaces = generator.generateSurfaces(runwayEndData);
 *   auto geojson = surfaces.toGeoJSON();
 */
class OLSSurfaceGenerator {
public:
    OLSSurfaceGenerator();
    ~OLSSurfaceGenerator() = default;
    
    /**
     * Generate all OLS surfaces for a runway end
     * @param runway_end Runway end data with all required parameters
     * @return Complete set of OLS surfaces
     */
    RunwayOLSSurfaces generateSurfaces(const RunwayEndData& runway_end);
    
    /**
     * Generate OLS surfaces for both ends of a runway
     * @param runway AirportRunway data from database
     * @param airport_elevation_ft Airport reference elevation
     * @param approach_category_le Approach category for low-end
     * @param approach_category_he Approach category for high-end
     * @return Vector of surface sets (one per runway end)
     */
    std::vector<RunwayOLSSurfaces> generateSurfacesForRunway(
        const AirportRunway& runway,
        int airport_elevation_ft,
        ApproachCategory approach_category_le = ApproachCategory::NonPrecisionApproach,
        ApproachCategory approach_category_he = ApproachCategory::NonPrecisionApproach
    );
    
    /**
     * Get OLS parameters for given runway classification
     * @param code_number Runway code number (1-4)
     * @param code_letter Runway code letter (A-F)
     * @param approach_category Approach type
     * @return OLS parameter set
     */
    OLSParameters getOLSParameters(
        RunwayCodeNumber code_number,
        RunwayCodeLetter code_letter,
        ApproachCategory approach_category
    );
    
    /**
     * Determine runway code number from length
     * @param length_m Runway length in meters
     * @return Appropriate code number
     */
    static RunwayCodeNumber determineCodeNumber(double length_m);
    
    /**
     * Determine runway code letter from width
     * @param width_m Runway width in meters
     * @return Appropriate code letter
     */
    static RunwayCodeLetter determineCodeLetter(double width_m);
    
    /**
     * Check if a point penetrates any OLS surface
     * @param point 3D point to check
     * @param surfaces OLS surfaces to check against
     * @return Penetration depth in meters (negative = below surface)
     */
    double checkPenetration(const GeoPoint3D& point, const RunwayOLSSurfaces& surfaces);
    
private:
    // Surface generation methods
    OLSSurface generateApproachSurface(const RunwayEndData& rwy, const OLSParameters& params);
    OLSSurface generateTakeoffClimbSurface(const RunwayEndData& rwy, const OLSParameters& params);
    OLSSurface generateTransitionalSurface(const RunwayEndData& rwy, const OLSParameters& params);
    OLSSurface generateInnerHorizontalSurface(const RunwayEndData& rwy, const OLSParameters& params);
    OLSSurface generateConicalSurface(const RunwayEndData& rwy, const OLSParameters& params);
    OLSSurface generateOuterHorizontalSurface(const RunwayEndData& rwy, const OLSParameters& params);
    OLSSurface generateInnerApproachSurface(const RunwayEndData& rwy, const OLSParameters& params);
    OLSSurface generateInnerTransitionalSurface(const RunwayEndData& rwy, const OLSParameters& params);
    OLSSurface generateBalkedLandingSurface(const RunwayEndData& rwy, const OLSParameters& params);
    
    // Coordinate transformation utilities
    GeoPoint3D projectPoint(const GeoPoint3D& origin, double bearing_deg, double distance_m);
    GeoPoint3D projectPointWithElevation(const GeoPoint3D& origin, double bearing_deg, 
                                         double distance_m, double slope_percent);
    std::vector<GeoPoint3D> generateArc(const GeoPoint3D& center, double radius_m, 
                                        double start_bearing, double end_bearing, 
                                        int num_points, double elevation);
    std::vector<GeoPoint3D> generateTrapezoid(const GeoPoint3D& origin, double heading_deg,
                                              double inner_width, double outer_width,
                                              double length, double base_elevation,
                                              double slope_percent);
    
    // Utility functions
    double degreesToRadians(double degrees);
    double radiansToDegrees(double radians);
    double normalizeHeading(double heading);
    double calculateDistance(const GeoPoint3D& p1, const GeoPoint3D& p2);
    double calculateBearing(const GeoPoint3D& from, const GeoPoint3D& to);
    
    // Constants
    static constexpr double EARTH_RADIUS_M = 6371000.0;
    static constexpr double METERS_PER_FOOT = 0.3048;
    static constexpr double FEET_PER_METER = 3.28084;
    static constexpr double PI = 3.14159265358979323846;
    
    // Default arc resolution for circular surfaces
    int arc_resolution_ = 72;  // Points per full circle
};

// ============================================================================
// Inline Helper Functions
// ============================================================================

inline double OLSSurfaceGenerator::degreesToRadians(double degrees) {
    return degrees * PI / 180.0;
}

inline double OLSSurfaceGenerator::radiansToDegrees(double radians) {
    return radians * 180.0 / PI;
}

inline double OLSSurfaceGenerator::normalizeHeading(double heading) {
    while (heading < 0) heading += 360.0;
    while (heading >= 360.0) heading -= 360.0;
    return heading;
}

} // namespace aeronautical
