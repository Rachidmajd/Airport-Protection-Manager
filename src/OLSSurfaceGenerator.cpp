#include "OLSSurfaceGenerator.h"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <cmath>

namespace aeronautical {

// ============================================================================
// OLSSurface Implementation
// ============================================================================

nlohmann::json OLSSurface::toGeoJSON() const {
    nlohmann::json feature;
    feature["type"] = "Feature";
    
    // Create polygon coordinates (2D)
    nlohmann::json coordinates = nlohmann::json::array();
    nlohmann::json ring = nlohmann::json::array();
    
    for (const auto& point : boundary_points) {
        ring.push_back({point.longitude, point.latitude});
    }
    
    // Close the ring if not already closed
    if (!boundary_points.empty() && 
        (boundary_points.front().latitude != boundary_points.back().latitude ||
         boundary_points.front().longitude != boundary_points.back().longitude)) {
        ring.push_back({boundary_points.front().longitude, boundary_points.front().latitude});
    }
    
    coordinates.push_back(ring);
    
    feature["geometry"] = {
        {"type", "Polygon"},
        {"coordinates", coordinates}
    };
    
    // Properties
    nlohmann::json props = properties;
    props["name"] = name;
    props["surface_type"] = surface_type;
    props["runway_designator"] = runway_designator;
    props["base_elevation_m"] = base_elevation;
    props["slope_percent"] = slope;
    
    feature["properties"] = props;
    
    return feature;
}

nlohmann::json OLSSurface::toGeoJSON3D() const {
    nlohmann::json feature;
    feature["type"] = "Feature";
    
    // Create polygon coordinates with elevation (3D)
    nlohmann::json coordinates = nlohmann::json::array();
    nlohmann::json ring = nlohmann::json::array();
    
    for (const auto& point : boundary_points) {
        ring.push_back({point.longitude, point.latitude, point.elevation});
    }
    
    // Close the ring
    if (!boundary_points.empty() && 
        (boundary_points.front().latitude != boundary_points.back().latitude ||
         boundary_points.front().longitude != boundary_points.back().longitude)) {
        ring.push_back({boundary_points.front().longitude, 
                       boundary_points.front().latitude,
                       boundary_points.front().elevation});
    }
    
    coordinates.push_back(ring);
    
    feature["geometry"] = {
        {"type", "Polygon"},
        {"coordinates", coordinates}
    };
    
    // Properties
    nlohmann::json props = properties;
    props["name"] = name;
    props["surface_type"] = surface_type;
    props["runway_designator"] = runway_designator;
    props["base_elevation_m"] = base_elevation;
    props["slope_percent"] = slope;
    
    feature["properties"] = props;
    
    return feature;
}

// ============================================================================
// RunwayOLSSurfaces Implementation
// ============================================================================

nlohmann::json RunwayOLSSurfaces::toGeoJSON() const {
    nlohmann::json feature_collection;
    feature_collection["type"] = "FeatureCollection";
    
    nlohmann::json features = nlohmann::json::array();
    
    // Add primary surfaces
    features.push_back(approach_surface.toGeoJSON());
    features.push_back(takeoff_climb_surface.toGeoJSON());
    features.push_back(transitional_surface.toGeoJSON());
    features.push_back(inner_horizontal_surface.toGeoJSON());
    features.push_back(conical_surface.toGeoJSON());
    
    // Add optional precision approach surfaces
    if (outer_horizontal_surface) {
        features.push_back(outer_horizontal_surface->toGeoJSON());
    }
    if (inner_approach_surface) {
        features.push_back(inner_approach_surface->toGeoJSON());
    }
    if (inner_transitional_surface) {
        features.push_back(inner_transitional_surface->toGeoJSON());
    }
    if (balked_landing_surface) {
        features.push_back(balked_landing_surface->toGeoJSON());
    }
    
    feature_collection["features"] = features;
    
    // Add collection-level properties
    feature_collection["properties"] = {
        {"airport_icao", airport_icao},
        {"runway_designator", runway_designator},
        {"runway_elevation_m", runway_elevation_m}
    };
    
    return feature_collection;
}

// ============================================================================
// OLSSurfaceGenerator Implementation
// ============================================================================

OLSSurfaceGenerator::OLSSurfaceGenerator() {
    // Constructor - nothing special needed
}

RunwayCodeNumber OLSSurfaceGenerator::determineCodeNumber(double length_m) {
    if (length_m < 800) return RunwayCodeNumber::Code1;
    if (length_m < 1200) return RunwayCodeNumber::Code2;
    if (length_m < 1800) return RunwayCodeNumber::Code3;
    return RunwayCodeNumber::Code4;
}

RunwayCodeLetter OLSSurfaceGenerator::determineCodeLetter(double width_m) {
    // Based on typical runway width to aircraft wingspan relationship
    if (width_m < 18) return RunwayCodeLetter::A;
    if (width_m < 23) return RunwayCodeLetter::B;
    if (width_m < 30) return RunwayCodeLetter::C;
    if (width_m < 45) return RunwayCodeLetter::D;
    if (width_m < 52) return RunwayCodeLetter::E;
    return RunwayCodeLetter::F;
}

OLSParameters OLSSurfaceGenerator::getOLSParameters(
    RunwayCodeNumber code_number,
    RunwayCodeLetter code_letter,
    ApproachCategory approach_category) {
    
    OLSParameters params;
    params.code_number = code_number;
    params.code_letter = code_letter;
    params.approach_category = approach_category;
    
    int code = static_cast<int>(code_number);
    
    // ========================================================================
    // APPROACH SURFACE PARAMETERS (ICAO Annex 14, Table 4-1)
    // ========================================================================
    
    switch (approach_category) {
        case ApproachCategory::NonInstrument:
            // Non-instrument runway
            switch (code_number) {
                case RunwayCodeNumber::Code1:
                    params.approach.length_inner_edge = 60;
                    params.approach.distance_from_threshold = 30;
                    params.approach.divergence = 10;  // 10% each side
                    params.approach.first_section_length = 1600;
                    params.approach.first_section_slope = 5.0;  // 1:20
                    params.approach.total_length = 1600;
                    break;
                case RunwayCodeNumber::Code2:
                    params.approach.length_inner_edge = 80;
                    params.approach.distance_from_threshold = 60;
                    params.approach.divergence = 10;
                    params.approach.first_section_length = 2500;
                    params.approach.first_section_slope = 4.0;  // 1:25
                    params.approach.total_length = 2500;
                    break;
                case RunwayCodeNumber::Code3:
                case RunwayCodeNumber::Code4:
                    params.approach.length_inner_edge = 150;
                    params.approach.distance_from_threshold = 60;
                    params.approach.divergence = 10;
                    params.approach.first_section_length = 3000;
                    params.approach.first_section_slope = 3.33;  // 1:30
                    params.approach.total_length = 3000;
                    break;
            }
            params.approach.second_section_length = 0;
            params.approach.second_section_slope = 0;
            params.approach.horizontal_section_length = 0;
            break;
            
        case ApproachCategory::NonPrecisionApproach:
            // Non-precision approach runway
            switch (code_number) {
                case RunwayCodeNumber::Code1:
                case RunwayCodeNumber::Code2:
                    params.approach.length_inner_edge = 150;
                    params.approach.distance_from_threshold = 60;
                    params.approach.divergence = 15;
                    params.approach.first_section_length = 2500;
                    params.approach.first_section_slope = 3.33;
                    params.approach.total_length = 2500;
                    break;
                case RunwayCodeNumber::Code3:
                case RunwayCodeNumber::Code4:
                    params.approach.length_inner_edge = 300;
                    params.approach.distance_from_threshold = 60;
                    params.approach.divergence = 15;
                    params.approach.first_section_length = 3000;
                    params.approach.first_section_slope = 2.0;  // 1:50
                    params.approach.second_section_length = 3600;
                    params.approach.second_section_slope = 2.5;
                    params.approach.horizontal_section_length = 8400;
                    params.approach.total_length = 15000;
                    break;
            }
            break;
            
        case ApproachCategory::PrecisionCategoryI:
            // Precision CAT I
            params.approach.length_inner_edge = 300;
            params.approach.distance_from_threshold = 60;
            params.approach.divergence = 15;
            params.approach.first_section_length = 3000;
            params.approach.first_section_slope = 2.0;
            params.approach.second_section_length = 3600;
            params.approach.second_section_slope = 2.5;
            params.approach.horizontal_section_length = 8400;
            params.approach.total_length = 15000;
            break;
            
        case ApproachCategory::PrecisionCategoryII:
        case ApproachCategory::PrecisionCategoryIII:
            // Precision CAT II/III
            params.approach.length_inner_edge = 300;
            params.approach.distance_from_threshold = 60;
            params.approach.divergence = 15;
            params.approach.first_section_length = 3000;
            params.approach.first_section_slope = 2.0;
            params.approach.second_section_length = 3600;
            params.approach.second_section_slope = 2.5;
            params.approach.horizontal_section_length = 8400;
            params.approach.total_length = 15000;
            
            // Inner Approach Surface
            params.inner_approach = InnerApproachSurfaceParams{
                120,    // width
                60,     // distance from threshold
                900,    // length
                2.0     // slope
            };
            
            // Inner Transitional Surface
            params.inner_transitional = InnerTransitionalSurfaceParams{
                33.3    // slope (1:3)
            };
            
            // Balked Landing Surface
            params.balked_landing = BalkedLandingSurfaceParams{
                120,    // length_inner_edge (same as inner approach width)
                approach_category == ApproachCategory::PrecisionCategoryII ? 900.0 : 900.0,  // distance
                10,     // divergence
                approach_category == ApproachCategory::PrecisionCategoryII ? 3.33 : 2.5  // slope
            };
            break;
    }
    
    // ========================================================================
    // TAKE-OFF CLIMB SURFACE PARAMETERS (ICAO Annex 14, Table 4-2)
    // ========================================================================
    
    switch (code_number) {
        case RunwayCodeNumber::Code1:
            params.takeoff_climb.length_inner_edge = 60;
            params.takeoff_climb.distance_from_end = 30;
            params.takeoff_climb.divergence = 10;
            params.takeoff_climb.final_width = 380;
            params.takeoff_climb.length = 1600;
            params.takeoff_climb.slope = 5.0;
            break;
        case RunwayCodeNumber::Code2:
            params.takeoff_climb.length_inner_edge = 80;
            params.takeoff_climb.distance_from_end = 60;
            params.takeoff_climb.divergence = 10;
            params.takeoff_climb.final_width = 580;
            params.takeoff_climb.length = 2500;
            params.takeoff_climb.slope = 4.0;
            break;
        case RunwayCodeNumber::Code3:
        case RunwayCodeNumber::Code4:
            params.takeoff_climb.length_inner_edge = 180;
            params.takeoff_climb.distance_from_end = 60;
            params.takeoff_climb.divergence = 12.5;
            params.takeoff_climb.final_width = 1200;
            params.takeoff_climb.length = 15000;
            params.takeoff_climb.slope = 2.0;
            break;
    }
    
    // ========================================================================
    // TRANSITIONAL SURFACE PARAMETERS
    // ========================================================================
    
    params.transitional.slope = 14.3;  // 1:7 for all cases
    params.transitional.height_limit = 45;  // To inner horizontal height
    
    // ========================================================================
    // INNER HORIZONTAL SURFACE PARAMETERS
    // ========================================================================
    
    switch (code_number) {
        case RunwayCodeNumber::Code1:
            params.inner_horizontal.radius = 2000;
            params.inner_horizontal.height = 45;
            break;
        case RunwayCodeNumber::Code2:
            params.inner_horizontal.radius = 2500;
            params.inner_horizontal.height = 45;
            break;
        case RunwayCodeNumber::Code3:
        case RunwayCodeNumber::Code4:
            params.inner_horizontal.radius = 4000;
            params.inner_horizontal.height = 45;
            break;
    }
    
    // ========================================================================
    // CONICAL SURFACE PARAMETERS
    // ========================================================================
    
    params.conical.slope = 5.0;  // 1:20 for all cases
    
    switch (code_number) {
        case RunwayCodeNumber::Code1:
            params.conical.height = 35;
            break;
        case RunwayCodeNumber::Code2:
            params.conical.height = 55;
            break;
        case RunwayCodeNumber::Code3:
        case RunwayCodeNumber::Code4:
            params.conical.height = 100;
            break;
    }
    
    // ========================================================================
    // OUTER HORIZONTAL SURFACE (Precision approaches only)
    // ========================================================================
    
    if (approach_category >= ApproachCategory::PrecisionCategoryI) {
        params.outer_horizontal = OuterHorizontalSurfaceParams{
            15000,  // radius
            150     // height above aerodrome
        };
    }
    
    return params;
}

// ============================================================================
// Coordinate Transformation Utilities
// ============================================================================

GeoPoint3D OLSSurfaceGenerator::projectPoint(const GeoPoint3D& origin, 
                                              double bearing_deg, 
                                              double distance_m) {
    double bearing_rad = degreesToRadians(bearing_deg);
    double lat1_rad = degreesToRadians(origin.latitude);
    double lon1_rad = degreesToRadians(origin.longitude);
    
    double angular_distance = distance_m / EARTH_RADIUS_M;
    
    double lat2_rad = std::asin(
        std::sin(lat1_rad) * std::cos(angular_distance) +
        std::cos(lat1_rad) * std::sin(angular_distance) * std::cos(bearing_rad)
    );
    
    double lon2_rad = lon1_rad + std::atan2(
        std::sin(bearing_rad) * std::sin(angular_distance) * std::cos(lat1_rad),
        std::cos(angular_distance) - std::sin(lat1_rad) * std::sin(lat2_rad)
    );
    
    return GeoPoint3D(
        radiansToDegrees(lat2_rad),
        radiansToDegrees(lon2_rad),
        origin.elevation
    );
}

GeoPoint3D OLSSurfaceGenerator::projectPointWithElevation(
    const GeoPoint3D& origin,
    double bearing_deg,
    double distance_m,
    double slope_percent) {
    
    GeoPoint3D point = projectPoint(origin, bearing_deg, distance_m);
    
    // Calculate elevation change based on slope
    // Positive slope means surface rises as you move away from origin
    double elevation_change = distance_m * (slope_percent / 100.0);
    point.elevation = origin.elevation + elevation_change;
    
    return point;
}

std::vector<GeoPoint3D> OLSSurfaceGenerator::generateArc(
    const GeoPoint3D& center,
    double radius_m,
    double start_bearing,
    double end_bearing,
    int num_points,
    double elevation) {
    
    std::vector<GeoPoint3D> points;
    
    // Normalize bearings
    start_bearing = normalizeHeading(start_bearing);
    end_bearing = normalizeHeading(end_bearing);
    
    // Handle wrap-around
    double bearing_range = end_bearing - start_bearing;
    if (bearing_range < 0) {
        bearing_range += 360.0;
    }
    
    double bearing_step = bearing_range / (num_points - 1);
    
    for (int i = 0; i < num_points; i++) {
        double bearing = normalizeHeading(start_bearing + i * bearing_step);
        GeoPoint3D point = projectPoint(center, bearing, radius_m);
        point.elevation = elevation;
        points.push_back(point);
    }
    
    return points;
}

std::vector<GeoPoint3D> OLSSurfaceGenerator::generateTrapezoid(
    const GeoPoint3D& origin,
    double heading_deg,
    double inner_width,
    double outer_width,
    double length,
    double base_elevation,
    double slope_percent) {
    
    std::vector<GeoPoint3D> points;
    
    double left_bearing = normalizeHeading(heading_deg - 90);
    double right_bearing = normalizeHeading(heading_deg + 90);
    
    // Inner edge points (at origin)
    GeoPoint3D inner_left = projectPoint(origin, left_bearing, inner_width / 2);
    inner_left.elevation = base_elevation;
    
    GeoPoint3D inner_right = projectPoint(origin, right_bearing, inner_width / 2);
    inner_right.elevation = base_elevation;
    
    // Outer edge points (at length distance)
    GeoPoint3D far_center = projectPoint(origin, heading_deg, length);
    double outer_elevation = base_elevation + length * (slope_percent / 100.0);
    
    GeoPoint3D outer_left = projectPoint(far_center, left_bearing, outer_width / 2);
    outer_left.elevation = outer_elevation;
    
    GeoPoint3D outer_right = projectPoint(far_center, right_bearing, outer_width / 2);
    outer_right.elevation = outer_elevation;
    
    // Build polygon (clockwise)
    points.push_back(inner_left);
    points.push_back(outer_left);
    points.push_back(outer_right);
    points.push_back(inner_right);
    
    return points;
}

double OLSSurfaceGenerator::calculateDistance(const GeoPoint3D& p1, const GeoPoint3D& p2) {
    double lat1_rad = degreesToRadians(p1.latitude);
    double lat2_rad = degreesToRadians(p2.latitude);
    double delta_lat = degreesToRadians(p2.latitude - p1.latitude);
    double delta_lon = degreesToRadians(p2.longitude - p1.longitude);
    
    double a = std::sin(delta_lat / 2) * std::sin(delta_lat / 2) +
               std::cos(lat1_rad) * std::cos(lat2_rad) *
               std::sin(delta_lon / 2) * std::sin(delta_lon / 2);
    
    double c = 2 * std::atan2(std::sqrt(a), std::sqrt(1 - a));
    
    return EARTH_RADIUS_M * c;
}

double OLSSurfaceGenerator::calculateBearing(const GeoPoint3D& from, const GeoPoint3D& to) {
    double lat1_rad = degreesToRadians(from.latitude);
    double lat2_rad = degreesToRadians(to.latitude);
    double delta_lon = degreesToRadians(to.longitude - from.longitude);
    
    double y = std::sin(delta_lon) * std::cos(lat2_rad);
    double x = std::cos(lat1_rad) * std::sin(lat2_rad) -
               std::sin(lat1_rad) * std::cos(lat2_rad) * std::cos(delta_lon);
    
    double bearing_rad = std::atan2(y, x);
    
    return normalizeHeading(radiansToDegrees(bearing_rad));
}

// ============================================================================
// Surface Generation Methods
// ============================================================================

RunwayOLSSurfaces OLSSurfaceGenerator::generateSurfaces(const RunwayEndData& runway_end) {
    RunwayOLSSurfaces surfaces;
    
    surfaces.airport_icao = "";  // Set by caller
    surfaces.runway_designator = runway_end.designator;
    surfaces.runway_elevation_m = runway_end.threshold_elevation_m;
    
    // Get OLS parameters for this runway
    OLSParameters params = getOLSParameters(
        runway_end.code_number,
        runway_end.code_letter,
        runway_end.approach_category
    );
    
    // Generate primary surfaces
    surfaces.approach_surface = generateApproachSurface(runway_end, params);
    surfaces.takeoff_climb_surface = generateTakeoffClimbSurface(runway_end, params);
    surfaces.transitional_surface = generateTransitionalSurface(runway_end, params);
    surfaces.inner_horizontal_surface = generateInnerHorizontalSurface(runway_end, params);
    surfaces.conical_surface = generateConicalSurface(runway_end, params);
    
    // Generate optional precision approach surfaces
    if (params.outer_horizontal) {
        surfaces.outer_horizontal_surface = generateOuterHorizontalSurface(runway_end, params);
    }
    if (params.inner_approach) {
        surfaces.inner_approach_surface = generateInnerApproachSurface(runway_end, params);
    }
    if (params.inner_transitional) {
        surfaces.inner_transitional_surface = generateInnerTransitionalSurface(runway_end, params);
    }
    if (params.balked_landing) {
        surfaces.balked_landing_surface = generateBalkedLandingSurface(runway_end, params);
    }
    
    return surfaces;
}

std::vector<RunwayOLSSurfaces> OLSSurfaceGenerator::generateSurfacesForRunway(
    const AirportRunway& runway,
    int airport_elevation_ft,
    ApproachCategory approach_category_le,
    ApproachCategory approach_category_he) {
    
    std::vector<RunwayOLSSurfaces> result;
    
    double length_m = runway.length_ft * METERS_PER_FOOT;
    double width_m = runway.width_ft * METERS_PER_FOOT;
    double elevation_m = airport_elevation_ft * METERS_PER_FOOT;
    
    RunwayCodeNumber code_number = determineCodeNumber(length_m);
    RunwayCodeLetter code_letter = determineCodeLetter(width_m);
    
    // Low-end runway data
    RunwayEndData le_data;
    le_data.designator = runway.le_ident;
    le_data.threshold_lat = runway.le_latitude;
    le_data.threshold_lon = runway.le_longitude;
    le_data.threshold_elevation_m = elevation_m;
    le_data.heading_deg = runway.le_heading_deg;
    le_data.runway_length_m = length_m;
    le_data.runway_width_m = width_m;
    le_data.code_number = code_number;
    le_data.code_letter = code_letter;
    le_data.approach_category = approach_category_le;
    
    // High-end runway data
    RunwayEndData he_data;
    he_data.designator = runway.he_ident;
    he_data.threshold_lat = runway.he_latitude;
    he_data.threshold_lon = runway.he_longitude;
    he_data.threshold_elevation_m = elevation_m;
    he_data.heading_deg = runway.he_heading_deg;
    he_data.runway_length_m = length_m;
    he_data.runway_width_m = width_m;
    he_data.code_number = code_number;
    he_data.code_letter = code_letter;
    he_data.approach_category = approach_category_he;
    
    // Generate surfaces for both ends
    result.push_back(generateSurfaces(le_data));
    result.push_back(generateSurfaces(he_data));
    
    return result;
}

OLSSurface OLSSurfaceGenerator::generateApproachSurface(
    const RunwayEndData& rwy, 
    const OLSParameters& params) {
    
    OLSSurface surface;
    surface.name = "Approach Surface RWY " + rwy.designator;
    surface.surface_type = "approach";
    surface.runway_designator = rwy.designator;
    surface.base_elevation = rwy.threshold_elevation_m;
    surface.slope = params.approach.first_section_slope;
    
    GeoPoint3D threshold(rwy.threshold_lat, rwy.threshold_lon, rwy.threshold_elevation_m);
    
    // Approach surface extends in the opposite direction of landing
    // (aircraft approach from opposite heading)
    double approach_heading = normalizeHeading(rwy.heading_deg + 180);
    
    // Start point is at threshold displaced by distance_from_threshold
    GeoPoint3D start_point = projectPoint(threshold, approach_heading, 
                                          params.approach.distance_from_threshold);
    start_point.elevation = rwy.threshold_elevation_m;
    
    // Calculate inner edge half-width
    double inner_half_width = params.approach.length_inner_edge / 2.0;
    
    // Calculate divergence per meter
    double divergence_per_m = params.approach.divergence / 100.0;
    
    std::vector<GeoPoint3D> points;
    
    // Inner edge corners
    double left_bearing = normalizeHeading(approach_heading - 90);
    double right_bearing = normalizeHeading(approach_heading + 90);
    
    GeoPoint3D inner_left = projectPoint(start_point, left_bearing, inner_half_width);
    inner_left.elevation = rwy.threshold_elevation_m;
    
    GeoPoint3D inner_right = projectPoint(start_point, right_bearing, inner_half_width);
    inner_right.elevation = rwy.threshold_elevation_m;
    
    // For single-section surfaces (non-instrument, some non-precision)
    if (params.approach.second_section_length == 0) {
        // Simple trapezoid
        double outer_half_width = inner_half_width + 
                                  params.approach.first_section_length * divergence_per_m;
        
        GeoPoint3D far_center = projectPoint(start_point, approach_heading, 
                                             params.approach.first_section_length);
        double far_elevation = rwy.threshold_elevation_m + 
                              params.approach.first_section_length * 
                              (params.approach.first_section_slope / 100.0);
        
        GeoPoint3D outer_left = projectPoint(far_center, left_bearing, outer_half_width);
        outer_left.elevation = far_elevation;
        
        GeoPoint3D outer_right = projectPoint(far_center, right_bearing, outer_half_width);
        outer_right.elevation = far_elevation;
        
        points = {inner_right, inner_left, outer_left, outer_right};
    } else {
        // Multi-section approach surface (precision/NPA code 3/4)
        // First section
        double first_end_half_width = inner_half_width + 
                                      params.approach.first_section_length * divergence_per_m;
        double first_end_elevation = rwy.threshold_elevation_m + 
                                     params.approach.first_section_length * 
                                     (params.approach.first_section_slope / 100.0);
        
        GeoPoint3D first_end = projectPoint(start_point, approach_heading, 
                                            params.approach.first_section_length);
        
        GeoPoint3D first_left = projectPoint(first_end, left_bearing, first_end_half_width);
        first_left.elevation = first_end_elevation;
        
        GeoPoint3D first_right = projectPoint(first_end, right_bearing, first_end_half_width);
        first_right.elevation = first_end_elevation;
        
        // Second section
        double cumulative_length = params.approach.first_section_length + 
                                   params.approach.second_section_length;
        double second_end_half_width = inner_half_width + cumulative_length * divergence_per_m;
        double second_end_elevation = first_end_elevation + 
                                      params.approach.second_section_length * 
                                      (params.approach.second_section_slope / 100.0);
        
        GeoPoint3D second_end = projectPoint(start_point, approach_heading, cumulative_length);
        
        GeoPoint3D second_left = projectPoint(second_end, left_bearing, second_end_half_width);
        second_left.elevation = second_end_elevation;
        
        GeoPoint3D second_right = projectPoint(second_end, right_bearing, second_end_half_width);
        second_right.elevation = second_end_elevation;
        
        // Horizontal section (if present)
        if (params.approach.horizontal_section_length > 0) {
            double total_length = params.approach.total_length;
            double outer_half_width = inner_half_width + total_length * divergence_per_m;
            
            GeoPoint3D outer_end = projectPoint(start_point, approach_heading, total_length);
            
            GeoPoint3D outer_left = projectPoint(outer_end, left_bearing, outer_half_width);
            outer_left.elevation = second_end_elevation;  // Horizontal section
            
            GeoPoint3D outer_right = projectPoint(outer_end, right_bearing, outer_half_width);
            outer_right.elevation = second_end_elevation;
            
            points = {inner_right, inner_left, first_left, second_left, 
                     outer_left, outer_right, second_right, first_right};
        } else {
            points = {inner_right, inner_left, first_left, second_left, 
                     second_right, first_right};
        }
    }
    
    surface.boundary_points = points;
    
    surface.properties = {
        {"inner_edge_width", params.approach.length_inner_edge},
        {"total_length", params.approach.total_length},
        {"divergence_percent", params.approach.divergence},
        {"first_section_slope", params.approach.first_section_slope},
        {"approach_category", static_cast<int>(rwy.approach_category)}
    };
    
    return surface;
}

OLSSurface OLSSurfaceGenerator::generateTakeoffClimbSurface(
    const RunwayEndData& rwy, 
    const OLSParameters& params) {
    
    OLSSurface surface;
    surface.name = "Take-off Climb Surface RWY " + rwy.designator;
    surface.surface_type = "takeoff_climb";
    surface.runway_designator = rwy.designator;
    surface.base_elevation = rwy.threshold_elevation_m;
    surface.slope = params.takeoff_climb.slope;
    
    GeoPoint3D threshold(rwy.threshold_lat, rwy.threshold_lon, rwy.threshold_elevation_m);
    
    // Take-off climb extends in the direction of take-off (runway heading)
    double takeoff_heading = rwy.heading_deg;
    
    // Calculate the far end of runway
    GeoPoint3D runway_end = projectPoint(threshold, takeoff_heading, rwy.runway_length_m);
    
    // Start point is at runway end + distance_from_end
    GeoPoint3D start_point = projectPoint(runway_end, takeoff_heading, 
                                          params.takeoff_climb.distance_from_end);
    start_point.elevation = rwy.threshold_elevation_m;
    
    double inner_half_width = params.takeoff_climb.length_inner_edge / 2.0;
    double divergence_per_m = params.takeoff_climb.divergence / 100.0;
    
    // Calculate where divergence stops (at final_width)
    double max_divergence = (params.takeoff_climb.final_width / 2.0) - inner_half_width;
    double divergence_distance = max_divergence / divergence_per_m;
    
    double left_bearing = normalizeHeading(takeoff_heading - 90);
    double right_bearing = normalizeHeading(takeoff_heading + 90);
    
    std::vector<GeoPoint3D> points;
    
    // Inner edge
    GeoPoint3D inner_left = projectPoint(start_point, left_bearing, inner_half_width);
    inner_left.elevation = rwy.threshold_elevation_m;
    
    GeoPoint3D inner_right = projectPoint(start_point, right_bearing, inner_half_width);
    inner_right.elevation = rwy.threshold_elevation_m;
    
    if (divergence_distance >= params.takeoff_climb.length) {
        // Divergence continues to end
        double outer_half_width = inner_half_width + 
                                  params.takeoff_climb.length * divergence_per_m;
        double end_elevation = rwy.threshold_elevation_m + 
                              params.takeoff_climb.length * 
                              (params.takeoff_climb.slope / 100.0);
        
        GeoPoint3D far_center = projectPoint(start_point, takeoff_heading, 
                                             params.takeoff_climb.length);
        
        GeoPoint3D outer_left = projectPoint(far_center, left_bearing, outer_half_width);
        outer_left.elevation = end_elevation;
        
        GeoPoint3D outer_right = projectPoint(far_center, right_bearing, outer_half_width);
        outer_right.elevation = end_elevation;
        
        points = {inner_left, inner_right, outer_right, outer_left};
    } else {
        // Divergence stops at final_width, then parallel sides
        double mid_elevation = rwy.threshold_elevation_m + 
                              divergence_distance * (params.takeoff_climb.slope / 100.0);
        double end_elevation = rwy.threshold_elevation_m + 
                              params.takeoff_climb.length * 
                              (params.takeoff_climb.slope / 100.0);
        
        GeoPoint3D mid_center = projectPoint(start_point, takeoff_heading, divergence_distance);
        GeoPoint3D mid_left = projectPoint(mid_center, left_bearing, 
                                           params.takeoff_climb.final_width / 2.0);
        mid_left.elevation = mid_elevation;
        
        GeoPoint3D mid_right = projectPoint(mid_center, right_bearing, 
                                            params.takeoff_climb.final_width / 2.0);
        mid_right.elevation = mid_elevation;
        
        GeoPoint3D far_center = projectPoint(start_point, takeoff_heading, 
                                             params.takeoff_climb.length);
        GeoPoint3D outer_left = projectPoint(far_center, left_bearing, 
                                             params.takeoff_climb.final_width / 2.0);
        outer_left.elevation = end_elevation;
        
        GeoPoint3D outer_right = projectPoint(far_center, right_bearing, 
                                              params.takeoff_climb.final_width / 2.0);
        outer_right.elevation = end_elevation;
        
        points = {inner_left, inner_right, mid_right, outer_right, 
                 outer_left, mid_left};
    }
    
    surface.boundary_points = points;
    
    surface.properties = {
        {"inner_edge_width", params.takeoff_climb.length_inner_edge},
        {"final_width", params.takeoff_climb.final_width},
        {"length", params.takeoff_climb.length},
        {"slope_percent", params.takeoff_climb.slope}
    };
    
    return surface;
}

OLSSurface OLSSurfaceGenerator::generateTransitionalSurface(
    const RunwayEndData& rwy, 
    const OLSParameters& params) {
    
    OLSSurface surface;
    surface.name = "Transitional Surface RWY " + rwy.designator;
    surface.surface_type = "transitional";
    surface.runway_designator = rwy.designator;
    surface.base_elevation = rwy.threshold_elevation_m;
    surface.slope = params.transitional.slope;
    
    GeoPoint3D threshold(rwy.threshold_lat, rwy.threshold_lon, rwy.threshold_elevation_m);
    
    // Transitional surface extends from the side of the runway strip
    // to the inner horizontal surface
    double strip_half_width = rwy.strip_width_m / 2.0;
    double horizontal_height = params.inner_horizontal.height;
    
    // Width of transitional surface at top (horizontal distance)
    // slope = 14.3% means rise/run = 0.143, so run = rise/0.143
    double transition_width = horizontal_height / (params.transitional.slope / 100.0);
    
    // Build the polygon for both sides
    double left_bearing = normalizeHeading(rwy.heading_deg - 90);
    double right_bearing = normalizeHeading(rwy.heading_deg + 90);
    
    // Points along the runway
    // We need to create a surface that follows the runway strip edge
    // and slopes up to the inner horizontal surface
    
    // Simplified: create rectangles on each side
    // In reality, this should follow the approach surface edges too
    
    std::vector<GeoPoint3D> points;
    
    // Calculate runway extent including strip beyond ends
    double total_strip_length = rwy.runway_length_m + 2 * rwy.strip_length_beyond_end_m;
    
    // Start of strip (before threshold)
    GeoPoint3D strip_start = projectPoint(threshold, normalizeHeading(rwy.heading_deg + 180), 
                                          rwy.strip_length_beyond_end_m);
    
    // Four corners of the strip edge (inner boundary)
    GeoPoint3D inner_start_left = projectPoint(strip_start, left_bearing, strip_half_width);
    inner_start_left.elevation = rwy.threshold_elevation_m;
    
    GeoPoint3D inner_start_right = projectPoint(strip_start, right_bearing, strip_half_width);
    inner_start_right.elevation = rwy.threshold_elevation_m;
    
    GeoPoint3D strip_end = projectPoint(strip_start, rwy.heading_deg, total_strip_length);
    
    GeoPoint3D inner_end_left = projectPoint(strip_end, left_bearing, strip_half_width);
    inner_end_left.elevation = rwy.threshold_elevation_m;
    
    GeoPoint3D inner_end_right = projectPoint(strip_end, right_bearing, strip_half_width);
    inner_end_right.elevation = rwy.threshold_elevation_m;
    
    // Outer boundary (at inner horizontal height)
    double outer_distance = strip_half_width + transition_width;
    double top_elevation = rwy.threshold_elevation_m + horizontal_height;
    
    GeoPoint3D outer_start_left = projectPoint(strip_start, left_bearing, outer_distance);
    outer_start_left.elevation = top_elevation;
    
    GeoPoint3D outer_start_right = projectPoint(strip_start, right_bearing, outer_distance);
    outer_start_right.elevation = top_elevation;
    
    GeoPoint3D outer_end_left = projectPoint(strip_end, left_bearing, outer_distance);
    outer_end_left.elevation = top_elevation;
    
    GeoPoint3D outer_end_right = projectPoint(strip_end, right_bearing, outer_distance);
    outer_end_right.elevation = top_elevation;
    
    // Create a polygon that encompasses both transitional surfaces
    // (left and right of runway)
    points = {
        outer_start_left, outer_end_left, outer_end_right, outer_start_right,
        inner_start_right, inner_end_right, inner_end_left, inner_start_left
    };
    
    surface.boundary_points = points;
    
    surface.properties = {
        {"slope_percent", params.transitional.slope},
        {"strip_width", rwy.strip_width_m},
        {"height_limit", horizontal_height}
    };
    
    return surface;
}

OLSSurface OLSSurfaceGenerator::generateInnerHorizontalSurface(
    const RunwayEndData& rwy, 
    const OLSParameters& params) {
    
    OLSSurface surface;
    surface.name = "Inner Horizontal Surface RWY " + rwy.designator;
    surface.surface_type = "inner_horizontal";
    surface.runway_designator = rwy.designator;
    surface.base_elevation = rwy.threshold_elevation_m + params.inner_horizontal.height;
    surface.slope = 0;  // Horizontal surface
    
    GeoPoint3D threshold(rwy.threshold_lat, rwy.threshold_lon, rwy.threshold_elevation_m);
    
    // Inner horizontal surface is a circular/oval surface centered on runway
    // For simplicity, we create arcs centered on each runway end
    
    double radius = params.inner_horizontal.radius;
    double surface_elevation = rwy.threshold_elevation_m + params.inner_horizontal.height;
    
    // Calculate runway far end
    GeoPoint3D runway_end = projectPoint(threshold, rwy.heading_deg, rwy.runway_length_m);
    
    // Generate arcs from both ends
    int points_per_arc = arc_resolution_ / 2;
    
    // Arc from threshold end (approach side)
    double approach_heading = normalizeHeading(rwy.heading_deg + 180);
    auto arc1 = generateArc(threshold, radius, 
                           normalizeHeading(approach_heading - 90),
                           normalizeHeading(approach_heading + 90),
                           points_per_arc, surface_elevation);
    
    // Arc from runway end (departure side)
    auto arc2 = generateArc(runway_end, radius,
                           normalizeHeading(rwy.heading_deg - 90),
                           normalizeHeading(rwy.heading_deg + 90),
                           points_per_arc, surface_elevation);
    
    // Combine into a single polygon (stadium shape)
    std::vector<GeoPoint3D> points;
    points.insert(points.end(), arc1.begin(), arc1.end());
    points.insert(points.end(), arc2.begin(), arc2.end());
    
    surface.boundary_points = points;
    
    surface.properties = {
        {"radius", radius},
        {"height_above_aerodrome", params.inner_horizontal.height}
    };
    
    return surface;
}

OLSSurface OLSSurfaceGenerator::generateConicalSurface(
    const RunwayEndData& rwy, 
    const OLSParameters& params) {
    
    OLSSurface surface;
    surface.name = "Conical Surface RWY " + rwy.designator;
    surface.surface_type = "conical";
    surface.runway_designator = rwy.designator;
    surface.base_elevation = rwy.threshold_elevation_m + params.inner_horizontal.height;
    surface.slope = params.conical.slope;
    
    GeoPoint3D threshold(rwy.threshold_lat, rwy.threshold_lon, rwy.threshold_elevation_m);
    
    double inner_radius = params.inner_horizontal.radius;
    // Outer radius based on conical height and slope
    double conical_horizontal_extent = params.conical.height / (params.conical.slope / 100.0);
    double outer_radius = inner_radius + conical_horizontal_extent;
    
    double inner_elevation = rwy.threshold_elevation_m + params.inner_horizontal.height;
    double outer_elevation = inner_elevation + params.conical.height;
    
    // Calculate runway far end
    GeoPoint3D runway_end = projectPoint(threshold, rwy.heading_deg, rwy.runway_length_m);
    
    int points_per_arc = arc_resolution_ / 2;
    
    // Inner boundary (same as inner horizontal outer edge)
    double approach_heading = normalizeHeading(rwy.heading_deg + 180);
    
    auto inner_arc1 = generateArc(threshold, inner_radius,
                                  normalizeHeading(approach_heading - 90),
                                  normalizeHeading(approach_heading + 90),
                                  points_per_arc, inner_elevation);
    
    auto inner_arc2 = generateArc(runway_end, inner_radius,
                                  normalizeHeading(rwy.heading_deg - 90),
                                  normalizeHeading(rwy.heading_deg + 90),
                                  points_per_arc, inner_elevation);
    
    // Outer boundary
    auto outer_arc1 = generateArc(threshold, outer_radius,
                                  normalizeHeading(approach_heading - 90),
                                  normalizeHeading(approach_heading + 90),
                                  points_per_arc, outer_elevation);
    
    auto outer_arc2 = generateArc(runway_end, outer_radius,
                                  normalizeHeading(rwy.heading_deg - 90),
                                  normalizeHeading(rwy.heading_deg + 90),
                                  points_per_arc, outer_elevation);
    
    // Create ring polygon (outer - inner)
    std::vector<GeoPoint3D> points;
    
    // Outer ring
    points.insert(points.end(), outer_arc1.begin(), outer_arc1.end());
    points.insert(points.end(), outer_arc2.begin(), outer_arc2.end());
    
    surface.boundary_points = points;
    
    surface.properties = {
        {"inner_radius", inner_radius},
        {"outer_radius", outer_radius},
        {"slope_percent", params.conical.slope},
        {"height", params.conical.height}
    };
    
    return surface;
}

OLSSurface OLSSurfaceGenerator::generateOuterHorizontalSurface(
    const RunwayEndData& rwy, 
    const OLSParameters& params) {
    
    OLSSurface surface;
    surface.name = "Outer Horizontal Surface RWY " + rwy.designator;
    surface.surface_type = "outer_horizontal";
    surface.runway_designator = rwy.designator;
    surface.base_elevation = rwy.threshold_elevation_m + params.outer_horizontal->height;
    surface.slope = 0;
    
    GeoPoint3D threshold(rwy.threshold_lat, rwy.threshold_lon, rwy.threshold_elevation_m);
    GeoPoint3D runway_end = projectPoint(threshold, rwy.heading_deg, rwy.runway_length_m);
    GeoPoint3D center((threshold.latitude + runway_end.latitude) / 2,
                      (threshold.longitude + runway_end.longitude) / 2,
                      rwy.threshold_elevation_m + params.outer_horizontal->height);
    
    // Generate circular boundary
    auto points = generateArc(center, params.outer_horizontal->radius,
                             0, 360, arc_resolution_, 
                             rwy.threshold_elevation_m + params.outer_horizontal->height);
    
    surface.boundary_points = points;
    
    surface.properties = {
        {"radius", params.outer_horizontal->radius},
        {"height_above_aerodrome", params.outer_horizontal->height}
    };
    
    return surface;
}

OLSSurface OLSSurfaceGenerator::generateInnerApproachSurface(
    const RunwayEndData& rwy, 
    const OLSParameters& params) {
    
    OLSSurface surface;
    surface.name = "Inner Approach Surface RWY " + rwy.designator;
    surface.surface_type = "inner_approach";
    surface.runway_designator = rwy.designator;
    surface.base_elevation = rwy.threshold_elevation_m;
    surface.slope = params.inner_approach->slope;
    
    GeoPoint3D threshold(rwy.threshold_lat, rwy.threshold_lon, rwy.threshold_elevation_m);
    double approach_heading = normalizeHeading(rwy.heading_deg + 180);
    
    GeoPoint3D start_point = projectPoint(threshold, approach_heading,
                                          params.inner_approach->distance_from_threshold);
    start_point.elevation = rwy.threshold_elevation_m;
    
    double half_width = params.inner_approach->width / 2.0;
    double left_bearing = normalizeHeading(approach_heading - 90);
    double right_bearing = normalizeHeading(approach_heading + 90);
    
    // Inner edge
    GeoPoint3D inner_left = projectPoint(start_point, left_bearing, half_width);
    inner_left.elevation = rwy.threshold_elevation_m;
    
    GeoPoint3D inner_right = projectPoint(start_point, right_bearing, half_width);
    inner_right.elevation = rwy.threshold_elevation_m;
    
    // Outer edge
    double end_elevation = rwy.threshold_elevation_m + 
                          params.inner_approach->length * 
                          (params.inner_approach->slope / 100.0);
    
    GeoPoint3D far_center = projectPoint(start_point, approach_heading, 
                                         params.inner_approach->length);
    
    GeoPoint3D outer_left = projectPoint(far_center, left_bearing, half_width);
    outer_left.elevation = end_elevation;
    
    GeoPoint3D outer_right = projectPoint(far_center, right_bearing, half_width);
    outer_right.elevation = end_elevation;
    
    surface.boundary_points = {inner_right, inner_left, outer_left, outer_right};
    
    surface.properties = {
        {"width", params.inner_approach->width},
        {"length", params.inner_approach->length},
        {"slope_percent", params.inner_approach->slope}
    };
    
    return surface;
}

OLSSurface OLSSurfaceGenerator::generateInnerTransitionalSurface(
    const RunwayEndData& rwy, 
    const OLSParameters& params) {
    
    OLSSurface surface;
    surface.name = "Inner Transitional Surface RWY " + rwy.designator;
    surface.surface_type = "inner_transitional";
    surface.runway_designator = rwy.designator;
    surface.base_elevation = rwy.threshold_elevation_m;
    surface.slope = params.inner_transitional->slope;
    
    // Inner transitional extends from inner approach edges to inner horizontal
    // This is a complex surface - simplified implementation
    GeoPoint3D threshold(rwy.threshold_lat, rwy.threshold_lon, rwy.threshold_elevation_m);
    double approach_heading = normalizeHeading(rwy.heading_deg + 180);
    
    double inner_approach_half_width = params.inner_approach->width / 2.0;
    double transition_width = params.inner_horizontal.height / 
                             (params.inner_transitional->slope / 100.0);
    
    GeoPoint3D start_point = projectPoint(threshold, approach_heading,
                                          params.inner_approach->distance_from_threshold);
    
    double left_bearing = normalizeHeading(approach_heading - 90);
    double right_bearing = normalizeHeading(approach_heading + 90);
    
    // Create simplified rectangle for each side
    GeoPoint3D inner_start = projectPoint(start_point, left_bearing, inner_approach_half_width);
    inner_start.elevation = rwy.threshold_elevation_m;
    
    GeoPoint3D outer_start = projectPoint(start_point, left_bearing, 
                                          inner_approach_half_width + transition_width);
    outer_start.elevation = rwy.threshold_elevation_m + params.inner_horizontal.height;
    
    GeoPoint3D end_point = projectPoint(start_point, approach_heading, 
                                        params.inner_approach->length);
    
    GeoPoint3D inner_end = projectPoint(end_point, left_bearing, inner_approach_half_width);
    inner_end.elevation = rwy.threshold_elevation_m + 
                         params.inner_approach->length * (params.inner_approach->slope / 100.0);
    
    GeoPoint3D outer_end = projectPoint(end_point, left_bearing, 
                                        inner_approach_half_width + transition_width);
    outer_end.elevation = inner_end.elevation + params.inner_horizontal.height;
    
    // One side only (left) - mirror for complete surface
    surface.boundary_points = {inner_start, inner_end, outer_end, outer_start};
    
    surface.properties = {
        {"slope_percent", params.inner_transitional->slope}
    };
    
    return surface;
}

OLSSurface OLSSurfaceGenerator::generateBalkedLandingSurface(
    const RunwayEndData& rwy, 
    const OLSParameters& params) {
    
    OLSSurface surface;
    surface.name = "Balked Landing Surface RWY " + rwy.designator;
    surface.surface_type = "balked_landing";
    surface.runway_designator = rwy.designator;
    surface.base_elevation = rwy.threshold_elevation_m;
    surface.slope = params.balked_landing->slope;
    
    GeoPoint3D threshold(rwy.threshold_lat, rwy.threshold_lon, rwy.threshold_elevation_m);
    
    // Balked landing surface starts on the runway and extends in departure direction
    GeoPoint3D start_point = projectPoint(threshold, rwy.heading_deg,
                                          params.balked_landing->distance_from_threshold);
    start_point.elevation = rwy.threshold_elevation_m;
    
    double inner_half_width = params.balked_landing->length_inner_edge / 2.0;
    double divergence_per_m = params.balked_landing->divergence / 100.0;
    
    // Length to inner horizontal height
    double length = params.inner_horizontal.height / (params.balked_landing->slope / 100.0);
    double outer_half_width = inner_half_width + length * divergence_per_m;
    
    double left_bearing = normalizeHeading(rwy.heading_deg - 90);
    double right_bearing = normalizeHeading(rwy.heading_deg + 90);
    
    GeoPoint3D inner_left = projectPoint(start_point, left_bearing, inner_half_width);
    inner_left.elevation = rwy.threshold_elevation_m;
    
    GeoPoint3D inner_right = projectPoint(start_point, right_bearing, inner_half_width);
    inner_right.elevation = rwy.threshold_elevation_m;
    
    GeoPoint3D far_center = projectPoint(start_point, rwy.heading_deg, length);
    double end_elevation = rwy.threshold_elevation_m + params.inner_horizontal.height;
    
    GeoPoint3D outer_left = projectPoint(far_center, left_bearing, outer_half_width);
    outer_left.elevation = end_elevation;
    
    GeoPoint3D outer_right = projectPoint(far_center, right_bearing, outer_half_width);
    outer_right.elevation = end_elevation;
    
    surface.boundary_points = {inner_right, inner_left, outer_left, outer_right};
    
    surface.properties = {
        {"inner_edge_width", params.balked_landing->length_inner_edge},
        {"slope_percent", params.balked_landing->slope},
        {"divergence_percent", params.balked_landing->divergence}
    };
    
    return surface;
}

double OLSSurfaceGenerator::checkPenetration(const GeoPoint3D& point, 
                                              const RunwayOLSSurfaces& surfaces) {
    // Simplified penetration check
    // Returns positive value if point penetrates any surface
    // Returns negative value (clearance) if point is below all surfaces
    
    double max_penetration = -9999;  // Large negative = lots of clearance
    
    // Helper lambda to check penetration against a single surface
    auto checkSurface = [&](const OLSSurface& surface) {
        // Simple check: is point within horizontal bounds?
        // If so, calculate expected surface elevation and compare
        
        // For now, use a simplified point-in-polygon test
        // and linear interpolation for sloped surfaces
        
        // This is a placeholder - real implementation needs proper 
        // 3D geometry intersection
        
        // Calculate surface elevation at point location
        // based on slope and distance from surface origin
        
        double surface_elevation_at_point = surface.base_elevation;
        
        // Add slope-based elevation if applicable
        if (surface.slope > 0 && !surface.boundary_points.empty()) {
            // Calculate distance from base of surface
            double dist = calculateDistance(surface.boundary_points[0], point);
            surface_elevation_at_point += dist * (surface.slope / 100.0);
        }
        
        double penetration = point.elevation - surface_elevation_at_point;
        if (penetration > max_penetration) {
            max_penetration = penetration;
        }
    };
    
    // Check all surfaces
    checkSurface(surfaces.approach_surface);
    checkSurface(surfaces.takeoff_climb_surface);
    checkSurface(surfaces.transitional_surface);
    checkSurface(surfaces.inner_horizontal_surface);
    checkSurface(surfaces.conical_surface);
    
    if (surfaces.outer_horizontal_surface) {
        checkSurface(*surfaces.outer_horizontal_surface);
    }
    if (surfaces.inner_approach_surface) {
        checkSurface(*surfaces.inner_approach_surface);
    }
    if (surfaces.inner_transitional_surface) {
        checkSurface(*surfaces.inner_transitional_surface);
    }
    if (surfaces.balked_landing_surface) {
        checkSurface(*surfaces.balked_landing_surface);
    }
    
    return max_penetration;
}

} // namespace aeronautical
