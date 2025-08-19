#include "FlightProcedure.h"
#include <sstream>
#include <iomanip>
#include <ctime>

namespace aeronautical {

// Enum conversion functions
std::string procedureTypeToString(ProcedureType type) {
    switch (type) {
        case ProcedureType::SID: return "SID";
        case ProcedureType::STAR: return "STAR";
        case ProcedureType::APPROACH: return "APPROACH";
        case ProcedureType::DEPARTURE: return "DEPARTURE";
        case ProcedureType::ARRIVAL: return "ARRIVAL";
        default: return "SID";
    }
}

ProcedureType stringToProcedureType(const std::string& str) {
    if (str == "STAR") return ProcedureType::STAR;
    if (str == "APPROACH") return ProcedureType::APPROACH;
    if (str == "DEPARTURE") return ProcedureType::DEPARTURE;
    if (str == "ARRIVAL") return ProcedureType::ARRIVAL;
    return ProcedureType::SID;
}

std::string altitudeRestrictionToString(AltitudeRestriction restriction) {
    switch (restriction) {
        case AltitudeRestriction::At: return "at";
        case AltitudeRestriction::AtOrAbove: return "at_or_above";
        case AltitudeRestriction::AtOrBelow: return "at_or_below";
        case AltitudeRestriction::Between: return "between";
        default: return "at";
    }
}

AltitudeRestriction stringToAltitudeRestriction(const std::string& str) {
    if (str == "at_or_above") return AltitudeRestriction::AtOrAbove;
    if (str == "at_or_below") return AltitudeRestriction::AtOrBelow;
    if (str == "between") return AltitudeRestriction::Between;
    return AltitudeRestriction::At;
}

std::string speedRestrictionToString(SpeedRestriction restriction) {
    switch (restriction) {
        case SpeedRestriction::At: return "at";
        case SpeedRestriction::AtOrBelow: return "at_or_below";
        case SpeedRestriction::AtOrAbove: return "at_or_above";
        default: return "at";
    }
}

SpeedRestriction stringToSpeedRestriction(const std::string& str) {
    if (str == "at_or_below") return SpeedRestriction::AtOrBelow;
    if (str == "at_or_above") return SpeedRestriction::AtOrAbove;
    return SpeedRestriction::At;
}

std::string turnDirectionToString(TurnDirection direction) {
    switch (direction) {
        case TurnDirection::Left: return "left";
        case TurnDirection::Right: return "right";
        case TurnDirection::Straight: return "straight";
        default: return "straight";
    }
}

TurnDirection stringToTurnDirection(const std::string& str) {
    if (str == "left") return TurnDirection::Left;
    if (str == "right") return TurnDirection::Right;
    return TurnDirection::Straight;
}

std::string protectionTypeToString(ProtectionType type) {
    switch (type) {
        case ProtectionType::OverallPrimary: return "overall_primary";
        case ProtectionType::OverallSecondary: return "overall_secondary";
        case ProtectionType::NoiseAbatement: return "noise_abatement";
        case ProtectionType::Environmental: return "environmental";
        case ProtectionType::ObstacleClearance: return "obstacle_clearance";
        case ProtectionType::TerrainClearance: return "terrain_clearance";
        case ProtectionType::CommunicationZone: return "communication_zone";
        case ProtectionType::SurveillanceZone: return "surveillance_zone";
        case ProtectionType::BufferZone: return "buffer_zone";
        case ProtectionType::RestrictedArea: return "restricted_area";
        default: return "overall_primary";
    }
}

ProtectionType stringToProtectionType(const std::string& str) {
    if (str == "overall_secondary") return ProtectionType::OverallSecondary;
    if (str == "noise_abatement") return ProtectionType::NoiseAbatement;
    if (str == "environmental") return ProtectionType::Environmental;
    if (str == "obstacle_clearance") return ProtectionType::ObstacleClearance;
    if (str == "terrain_clearance") return ProtectionType::TerrainClearance;
    if (str == "communication_zone") return ProtectionType::CommunicationZone;
    if (str == "surveillance_zone") return ProtectionType::SurveillanceZone;
    if (str == "buffer_zone") return ProtectionType::BufferZone;
    if (str == "restricted_area") return ProtectionType::RestrictedArea;
    return ProtectionType::OverallPrimary;
}

std::string restrictionLevelToString(RestrictionLevel level) {
    switch (level) {
        case RestrictionLevel::Prohibited: return "prohibited";
        case RestrictionLevel::Restricted: return "restricted";
        case RestrictionLevel::Caution: return "caution";
        case RestrictionLevel::Advisory: return "advisory";
        case RestrictionLevel::Monitoring: return "monitoring";
        default: return "restricted";
    }
}

RestrictionLevel stringToRestrictionLevel(const std::string& str) {
    if (str == "prohibited") return RestrictionLevel::Prohibited;
    if (str == "caution") return RestrictionLevel::Caution;
    if (str == "advisory") return RestrictionLevel::Advisory;
    if (str == "monitoring") return RestrictionLevel::Monitoring;
    return RestrictionLevel::Restricted;
}

std::string conflictSeverityToString(ConflictSeverity severity) {
    switch (severity) {
        case ConflictSeverity::Critical: return "critical";
        case ConflictSeverity::High: return "high";
        case ConflictSeverity::Medium: return "medium";
        case ConflictSeverity::Low: return "low";
        case ConflictSeverity::Informational: return "informational";
        default: return "medium";
    }
}

ConflictSeverity stringToConflictSeverity(const std::string& str) {
    if (str == "critical") return ConflictSeverity::Critical;
    if (str == "high") return ConflictSeverity::High;
    if (str == "low") return ConflictSeverity::Low;
    if (str == "informational") return ConflictSeverity::Informational;
    return ConflictSeverity::Medium;
}

std::string altitudeReferenceToString(AltitudeReference ref) {
    switch (ref) {
        case AltitudeReference::MSL: return "MSL";
        case AltitudeReference::AGL: return "AGL";
        case AltitudeReference::FL: return "FL";
        default: return "MSL";
    }
}

AltitudeReference stringToAltitudeReference(const std::string& str) {
    if (str == "AGL") return AltitudeReference::AGL;
    if (str == "FL") return AltitudeReference::FL;
    return AltitudeReference::MSL;
}

// ProcedureSegment implementation
nlohmann::json ProcedureSegment::toJson() const {
    nlohmann::json j;
    j["id"] = id;
    j["procedure_id"] = procedure_id;
    j["segment_order"] = segment_order;
    if (segment_name) j["segment_name"] = *segment_name;
    if (waypoint_from) j["waypoint_from"] = *waypoint_from;
    if (waypoint_to) j["waypoint_to"] = *waypoint_to;
    if (altitude_min) j["altitude_min"] = *altitude_min;
    if (altitude_max) j["altitude_max"] = *altitude_max;
    if (altitude_restriction) j["altitude_restriction"] = altitudeRestrictionToString(*altitude_restriction);
    if (speed_limit) j["speed_limit"] = *speed_limit;
    if (speed_restriction) j["speed_restriction"] = speedRestrictionToString(*speed_restriction);
    
    // Parse trajectory_geometry as JSON
    try {
        j["trajectory_geometry"] = nlohmann::json::parse(trajectory_geometry);
    } catch (...) {
        j["trajectory_geometry"] = trajectory_geometry;
    }
    
    if (segment_length) j["segment_length"] = *segment_length;
    if (magnetic_course) j["magnetic_course"] = *magnetic_course;
    j["turn_direction"] = turnDirectionToString(turn_direction);
    j["is_mandatory"] = is_mandatory;
    
    return j;
}

ProcedureSegment ProcedureSegment::fromJson(const nlohmann::json& j) {
    ProcedureSegment s;
    
    if (j.contains("id")) s.id = j["id"];
    if (j.contains("procedure_id")) s.procedure_id = j["procedure_id"];
    if (j.contains("segment_order")) s.segment_order = j["segment_order"];
    if (j.contains("segment_name") && !j["segment_name"].is_null()) 
        s.segment_name = j["segment_name"];
    if (j.contains("waypoint_from") && !j["waypoint_from"].is_null()) 
        s.waypoint_from = j["waypoint_from"];
    if (j.contains("waypoint_to") && !j["waypoint_to"].is_null()) 
        s.waypoint_to = j["waypoint_to"];
    if (j.contains("altitude_min") && !j["altitude_min"].is_null()) 
        s.altitude_min = j["altitude_min"];
    if (j.contains("altitude_max") && !j["altitude_max"].is_null()) 
        s.altitude_max = j["altitude_max"];
    if (j.contains("altitude_restriction") && !j["altitude_restriction"].is_null()) 
        s.altitude_restriction = stringToAltitudeRestriction(j["altitude_restriction"]);
    if (j.contains("speed_limit") && !j["speed_limit"].is_null()) 
        s.speed_limit = j["speed_limit"];
    if (j.contains("speed_restriction") && !j["speed_restriction"].is_null()) 
        s.speed_restriction = stringToSpeedRestriction(j["speed_restriction"]);
    if (j.contains("trajectory_geometry")) {
        if (j["trajectory_geometry"].is_string()) {
            s.trajectory_geometry = j["trajectory_geometry"];
        } else {
            s.trajectory_geometry = j["trajectory_geometry"].dump();
        }
    }
    if (j.contains("segment_length") && !j["segment_length"].is_null()) 
        s.segment_length = j["segment_length"];
    if (j.contains("magnetic_course") && !j["magnetic_course"].is_null()) 
        s.magnetic_course = j["magnetic_course"];
    if (j.contains("turn_direction")) 
        s.turn_direction = stringToTurnDirection(j["turn_direction"]);
    if (j.contains("is_mandatory")) 
        s.is_mandatory = j["is_mandatory"];
    
    return s;
}

// ProcedureProtection implementation
nlohmann::json ProcedureProtection::toJson() const {
    nlohmann::json j;
    j["id"] = id;
    j["procedure_id"] = procedure_id;
    j["protection_name"] = protection_name;
    j["protection_type"] = protectionTypeToString(protection_type);
    if (description) j["description"] = *description;
    
    // Parse protection_geometry as JSON
    try {
        j["protection_geometry"] = nlohmann::json::parse(protection_geometry);
    } catch (...) {
        j["protection_geometry"] = protection_geometry;
    }
    
    if (altitude_min) j["altitude_min"] = *altitude_min;
    if (altitude_max) j["altitude_max"] = *altitude_max;
    j["altitude_reference"] = altitudeReferenceToString(altitude_reference);
    if (area_size) j["area_size"] = *area_size;
    if (center_lat) j["center_lat"] = *center_lat;
    if (center_lng) j["center_lng"] = *center_lng;
    if (buffer_distance) j["buffer_distance"] = *buffer_distance;
    j["restriction_level"] = restrictionLevelToString(restriction_level);
    j["conflict_severity"] = conflictSeverityToString(conflict_severity);
    j["analysis_priority"] = analysis_priority;
    if (time_restriction) j["time_restriction"] = *time_restriction;
    j["weather_dependent"] = weather_dependent;
    if (regulatory_source) j["regulatory_source"] = *regulatory_source;
    if (operational_notes) j["operational_notes"] = *operational_notes;
    if (contact_info) j["contact_info"] = *contact_info;
    j["is_active"] = is_active;
    if (effective_date) j["effective_date"] = timePointToString(*effective_date);
    if (expiry_date) j["expiry_date"] = timePointToString(*expiry_date);
    if (review_date) j["review_date"] = timePointToString(*review_date);
    j["created_at"] = timePointToString(created_at);
    j["updated_at"] = timePointToString(updated_at);
    if (created_by) j["created_by"] = *created_by;
    if (last_reviewed_by) j["last_reviewed_by"] = *last_reviewed_by;
    if (last_review_date) j["last_review_date"] = timePointToString(*last_review_date);
    
    return j;
}

ProcedureProtection ProcedureProtection::fromJson(const nlohmann::json& j) {
    ProcedureProtection p;
    
    if (j.contains("id")) p.id = j["id"];
    if (j.contains("procedure_id")) p.procedure_id = j["procedure_id"];
    if (j.contains("protection_name")) p.protection_name = j["protection_name"];
    if (j.contains("protection_type")) p.protection_type = stringToProtectionType(j["protection_type"]);
    if (j.contains("description") && !j["description"].is_null()) 
        p.description = j["description"];
    if (j.contains("protection_geometry")) {
        if (j["protection_geometry"].is_string()) {
            p.protection_geometry = j["protection_geometry"];
        } else {
            p.protection_geometry = j["protection_geometry"].dump();
        }
    }
    if (j.contains("altitude_min") && !j["altitude_min"].is_null()) 
        p.altitude_min = j["altitude_min"];
    if (j.contains("altitude_max") && !j["altitude_max"].is_null()) 
        p.altitude_max = j["altitude_max"];
    if (j.contains("altitude_reference")) 
        p.altitude_reference = stringToAltitudeReference(j["altitude_reference"]);
    if (j.contains("area_size") && !j["area_size"].is_null()) 
        p.area_size = j["area_size"];
    if (j.contains("center_lat") && !j["center_lat"].is_null()) 
        p.center_lat = j["center_lat"];
    if (j.contains("center_lng") && !j["center_lng"].is_null()) 
        p.center_lng = j["center_lng"];
    if (j.contains("buffer_distance") && !j["buffer_distance"].is_null()) 
        p.buffer_distance = j["buffer_distance"];
    if (j.contains("restriction_level")) 
        p.restriction_level = stringToRestrictionLevel(j["restriction_level"]);
    if (j.contains("conflict_severity")) 
        p.conflict_severity = stringToConflictSeverity(j["conflict_severity"]);
    if (j.contains("analysis_priority")) 
        p.analysis_priority = j["analysis_priority"];
    if (j.contains("time_restriction") && !j["time_restriction"].is_null()) 
        p.time_restriction = j["time_restriction"];
    if (j.contains("weather_dependent")) 
        p.weather_dependent = j["weather_dependent"];
    if (j.contains("regulatory_source") && !j["regulatory_source"].is_null()) 
        p.regulatory_source = j["regulatory_source"];
    if (j.contains("operational_notes") && !j["operational_notes"].is_null()) 
        p.operational_notes = j["operational_notes"];
    if (j.contains("contact_info") && !j["contact_info"].is_null()) 
        p.contact_info = j["contact_info"];
    if (j.contains("is_active")) 
        p.is_active = j["is_active"];
    
    return p;
}

// FlightProcedure implementation
nlohmann::json FlightProcedure::toJson() const {
    nlohmann::json j;
    j["id"] = id;
    j["procedure_code"] = procedure_code;
    j["name"] = name;
    j["type"] = procedureTypeToString(type);
    j["airport_icao"] = airport_icao;
    if (runway) j["runway"] = *runway;
    if (description) j["description"] = *description;
    if (effective_date) j["effective_date"] = timePointToString(*effective_date);
    if (expiry_date) j["expiry_date"] = timePointToString(*expiry_date);
    j["is_active"] = is_active;
    j["created_at"] = timePointToString(created_at);
    j["updated_at"] = timePointToString(updated_at);

    if (trajectory_geometry) j["trajectory_geometry"] = *trajectory_geometry;
    if (protection_geometry) j["protection_geometry"] = *protection_geometry;
    
    // // Add segments and protections
    // j["segments"] = nlohmann::json::array();
    // for (const auto& segment : segments) {
    //     j["segments"].push_back(segment.toJson());
    // }
    
    // j["protections"] = nlohmann::json::array();
    // for (const auto& protection : protections) {
    //     j["protections"].push_back(protection.toJson());
    // }
    
    return j;
}

FlightProcedure FlightProcedure::fromJson(const nlohmann::json& j) {
    FlightProcedure p;
    
    if (j.contains("id")) p.id = j["id"];
    if (j.contains("procedure_code")) p.procedure_code = j["procedure_code"];
    if (j.contains("name")) p.name = j["name"];
    if (j.contains("type")) p.type = stringToProcedureType(j["type"]);
    if (j.contains("airport_icao")) p.airport_icao = j["airport_icao"];
    if (j.contains("runway") && !j["runway"].is_null()) 
        p.runway = j["runway"];
    if (j.contains("description") && !j["description"].is_null()) 
        p.description = j["description"];
    if (j.contains("effective_date") && !j["effective_date"].is_null()) 
        p.effective_date = stringToTimePoint(j["effective_date"]);
    if (j.contains("expiry_date") && !j["expiry_date"].is_null()) 
        p.expiry_date = stringToTimePoint(j["expiry_date"]);
    if (j.contains("is_active")) 
        p.is_active = j["is_active"];
    
    return p;
}

} // namespace aeronautical