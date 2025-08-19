#pragma once

#include <string>
#include <optional>
#include <chrono>
#include <vector>
#include <json.hpp>

namespace aeronautical {

enum class ProcedureType {
    SID,
    STAR,
    APPROACH,
    DEPARTURE,
    ARRIVAL
};

enum class AltitudeRestriction {
    At,
    AtOrAbove,
    AtOrBelow,
    Between
};

enum class SpeedRestriction {
    At,
    AtOrBelow,
    AtOrAbove
};

enum class TurnDirection {
    Left,
    Right,
    Straight
};

enum class ProtectionType {
    OverallPrimary,
    OverallSecondary,
    NoiseAbatement,
    Environmental,
    ObstacleClearance,
    TerrainClearance,
    CommunicationZone,
    SurveillanceZone,
    BufferZone,
    RestrictedArea
};

enum class RestrictionLevel {
    Prohibited,
    Restricted,
    Caution,
    Advisory,
    Monitoring
};

enum class ConflictSeverity {
    Critical,
    High,
    Medium,
    Low,
    Informational
};

enum class AltitudeReference {
    MSL,
    AGL,
    FL
};

// Convert enums to/from strings
std::string procedureTypeToString(ProcedureType type);
ProcedureType stringToProcedureType(const std::string& str);
std::string altitudeRestrictionToString(AltitudeRestriction restriction);
AltitudeRestriction stringToAltitudeRestriction(const std::string& str);
std::string speedRestrictionToString(SpeedRestriction restriction);
SpeedRestriction stringToSpeedRestriction(const std::string& str);
std::string turnDirectionToString(TurnDirection direction);
TurnDirection stringToTurnDirection(const std::string& str);
std::string protectionTypeToString(ProtectionType type);
ProtectionType stringToProtectionType(const std::string& str);
std::string restrictionLevelToString(RestrictionLevel level);
RestrictionLevel stringToRestrictionLevel(const std::string& str);
std::string conflictSeverityToString(ConflictSeverity severity);
ConflictSeverity stringToConflictSeverity(const std::string& str);
std::string altitudeReferenceToString(AltitudeReference ref);
AltitudeReference stringToAltitudeReference(const std::string& str);

struct ProcedureSegment {
    int id;
    int procedure_id;
    int segment_order;
    std::optional<std::string> segment_name;
    std::optional<std::string> waypoint_from;
    std::optional<std::string> waypoint_to;
    std::optional<int> altitude_min;
    std::optional<int> altitude_max;
    std::optional<AltitudeRestriction> altitude_restriction;
    std::optional<int> speed_limit;
    std::optional<SpeedRestriction> speed_restriction;
    std::string trajectory_geometry; // GeoJSON LineString
    std::optional<double> segment_length;
    std::optional<int> magnetic_course;
    TurnDirection turn_direction = TurnDirection::Straight;
    bool is_mandatory = true;
    
    nlohmann::json toJson() const;
    static ProcedureSegment fromJson(const nlohmann::json& j);
};

struct ProcedureProtection {
    int id;
    int procedure_id;
    std::string protection_name;
    ProtectionType protection_type;
    std::optional<std::string> description;
    std::string protection_geometry; // GeoJSON Polygon
    std::optional<int> altitude_min;
    std::optional<int> altitude_max;
    AltitudeReference altitude_reference = AltitudeReference::MSL;
    std::optional<double> area_size;
    std::optional<double> center_lat;
    std::optional<double> center_lng;
    std::optional<double> buffer_distance;
    RestrictionLevel restriction_level = RestrictionLevel::Restricted;
    ConflictSeverity conflict_severity = ConflictSeverity::Medium;
    int analysis_priority = 50;
    std::optional<std::string> time_restriction;
    bool weather_dependent = false;
    std::optional<std::string> regulatory_source;
    std::optional<std::string> operational_notes;
    std::optional<std::string> contact_info;
    bool is_active = true;
    std::optional<std::chrono::system_clock::time_point> effective_date;
    std::optional<std::chrono::system_clock::time_point> expiry_date;
    std::optional<std::chrono::system_clock::time_point> review_date;
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point updated_at;
    std::optional<int> created_by;
    std::optional<int> last_reviewed_by;
    std::optional<std::chrono::system_clock::time_point> last_review_date;
    
    nlohmann::json toJson() const;
    static ProcedureProtection fromJson(const nlohmann::json& j);
};

struct FlightProcedure {
    int id;
    std::string procedure_code;
    std::string name;
    ProcedureType type;
    std::string airport_icao;
    std::optional<std::string> runway;
    std::optional<std::string> description;
    std::optional<std::chrono::system_clock::time_point> effective_date;
    std::optional<std::chrono::system_clock::time_point> expiry_date;
    bool is_active = true;
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point updated_at;
    


    // Related data
    // New fields replace segments/protections vectors
    std::optional<std::string> trajectory_geometry;  // GeoJSON LineString
    std::optional<std::string> protection_geometry;  // GeoJSON FeatureCollection
    
    // std::vector<ProcedureSegment> segments;
    // std::vector<ProcedureProtection> protections;
    
    nlohmann::json toJson() const;
    static FlightProcedure fromJson(const nlohmann::json& j);
};

// Helper for datetime conversion (reuse from Project.h)
std::string timePointToString(const std::chrono::system_clock::time_point& tp);
std::chrono::system_clock::time_point stringToTimePoint(const std::string& str);

} // namespace aeronautical