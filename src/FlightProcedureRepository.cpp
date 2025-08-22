#include "FlightProcedureRepository.h"
#include "DatabaseManager.h"
#include <sstream>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace aeronautical {

FlightProcedureRepository::FlightProcedureRepository() {
    // Fixed spdlog initialization
    try {
        logger_ = spdlog::get("aeronautical");
        if (!logger_) {
            // Create a console logger with color support
            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            logger_ = std::make_shared<spdlog::logger>("aeronautical", console_sink);
            spdlog::register_logger(logger_);
        }
    } catch (const std::exception& e) {
        // Fallback to default logger
        logger_ = spdlog::default_logger();
    }
}

#ifdef USE_MYSQL_C_API

std::string FlightProcedureRepository::buildSelectQuery() const {
    // return "SELECT fp.id, fp.procedure_code, fp.name, fp.type, fp.airport_icao, "
    //        "fp.runway, fp.description, fp.effective_date, fp.expiry_date, "
    //        "fp.is_active, fp.created_at, fp.updated_at "
    //        "FROM flight_procedures fp";
    return "SELECT fp.id, fp.procedure_code, fp.name, fp.type, "
           "fp.airport_icao, fp.runway, fp.description, "
           "fp.trajectory_geometry, fp.protection_geometry, "
           "fp.effective_date, fp.expiry_date, fp.is_active "
           "FROM flight_procedures fp";

        //        return "SELECT fp.id, fp.procedure_code, fp.name, fp.type, "
        //    "fp.airport_icao, fp.runway, fp.description, "
        //    "fp.effective_date, fp.expiry_date, fp.is_active "
        //    "FROM flight_procedures fp";
}

std::string FlightProcedureRepository::buildSegmentSelectQuery() const {
    return "SELECT ps.id, ps.procedure_id, ps.segment_order, ps.segment_name, "
           "ps.waypoint_from, ps.waypoint_to, ps.altitude_min, ps.altitude_max, "
           "ps.altitude_restriction, ps.speed_limit, ps.speed_restriction, "
           "ps.trajectory_geometry, ps.segment_length, ps.magnetic_course, "
           "ps.turn_direction, ps.is_mandatory "
           "FROM procedure_segments ps";
}

std::string FlightProcedureRepository::buildProtectionSelectQuery() const {
    return "SELECT fpp.id, fpp.procedure_id, fpp.protection_name, fpp.protection_type, "
           "fpp.description, fpp.protection_geometry, fpp.altitude_min, fpp.altitude_max, "
           "fpp.altitude_reference, fpp.area_size, fpp.center_lat, fpp.center_lng, "
           "fpp.buffer_distance, fpp.restriction_level, fpp.conflict_severity, "
           "fpp.analysis_priority, fpp.time_restriction, fpp.weather_dependent, "
           "fpp.regulatory_source, fpp.operational_notes, fpp.contact_info, "
           "fpp.is_active, fpp.effective_date, fpp.expiry_date, fpp.review_date, "
           "fpp.created_at, fpp.updated_at, fpp.created_by, fpp.last_reviewed_by, "
           "fpp.last_review_date "
           "FROM flight_procedure_protection fpp";
}

std::string FlightProcedureRepository::buildCountQuery() const {
    return "SELECT COUNT(*) FROM flight_procedures fp";
}

// std::vector<FlightProcedure> FlightProcedureRepository::findAll(const FlightProcedureFilter& filter) {
//     std::vector<FlightProcedure> procedures;
    
//     try {
//         auto& db = DatabaseManager::getInstance();
        
//         // First check if the table exists
//         MYSQL_RES* tableCheck = db.executeSelectQuery("SHOW TABLES LIKE 'flight_procedures'");
//         if (!tableCheck) {
//             logger_->warn("Cannot check if flight_procedures table exists");
//             return procedures;
//         }
        
//         MYSQL_ROW tableRow = mysql_fetch_row(tableCheck);
//         if (!tableRow) {
//             logger_->warn("flight_procedures table does not exist, returning empty list");
//             mysql_free_result(tableCheck);
//             return procedures;
//         }
//         mysql_free_result(tableCheck);
        
//         std::stringstream query;
//         query << buildSelectQuery() << " WHERE 1=1";
        
//         if (filter.type) {
//             query << " AND fp.type = '" << procedureTypeToString(*filter.type) << "'";
//         }
        
//         if (filter.airport_icao) {
//             query << " AND fp.airport_icao = '" << *filter.airport_icao << "'";
//         }
        
//         if (filter.runway) {
//             query << " AND fp.runway = '" << *filter.runway << "'";
//         }
        
//         if (filter.is_active) {
//             query << " AND fp.is_active = " << (*filter.is_active ? 1 : 0);
//         }
        
//         query << " ORDER BY fp.procedure_code ASC";
//         query << " LIMIT " << filter.limit << " OFFSET " << filter.offset;
        
//         logger_->debug("Executing flight procedures query: {}", query.str());
        
//         MYSQL_RES* result = db.executeSelectQuery(query.str());
//         if (!result) {
//             logger_->warn("Flight procedures query failed, returning empty list");
//             return procedures;
//         }
        
//         MYSQL_ROW row;
//         while ((row = mysql_fetch_row(result))) {
//             unsigned long* lengths = mysql_fetch_lengths(result);
//             FlightProcedure procedure = rowToProcedure(row, lengths);
            
//             // Load related data if requested (with error handling)
//             try {
//                 loadRelatedData(procedure, filter.include_segments, filter.include_protections);
//             } catch (const std::exception& e) {
//                 logger_->warn("Failed to load related data for procedure {}: {}", procedure.procedure_code, e.what());
//                 // Continue without related data
//             }
            
//             procedures.push_back(procedure);
//         }
        
//         mysql_free_result(result);
        
//         logger_->debug("Found {} flight procedures", procedures.size());
//     } catch (const std::exception& err) {
//         logger_->error("Failed to fetch flight procedures: {}", err.what());
//         // Return empty list instead of throwing
//         return std::vector<FlightProcedure>();
//     }
    
//     return procedures;
// }

std::vector<FlightProcedure> FlightProcedureRepository::findAll(const FlightProcedureFilter& filter) {
    std::vector<FlightProcedure> procedures;
    
    try {
        auto& db = DatabaseManager::getInstance();
        
        // STEP 1: Update the query string to include the geometry fields
        std::string query = "SELECT fp.id, fp.procedure_code, fp.name, fp.type, "
                           "fp.airport_icao, fp.runway, fp.description, "
                           "fp.trajectory_geometry, fp.protection_geometry, " // <-- ADDED THESE
                           "fp.effective_date, fp.expiry_date, fp.is_active, "
                           "fp.created_at, fp.updated_at " // <-- Also ensure these are selected
                           "FROM flight_procedures fp WHERE 1=1";
        
        if (filter.is_active.has_value()) {
            query += " AND fp.is_active = " + std::to_string(*filter.is_active ? 1 : 0);
        }
        
        // Add other filters if you need them (from your original code)
        if (filter.airport_icao) {
            query += " AND fp.airport_icao = '" + *filter.airport_icao + "'";
        }

        query += " ORDER BY fp.procedure_code ASC";
        query += " LIMIT " + std::to_string(filter.limit) + " OFFSET " + std::to_string(filter.offset);
        
        logger_->debug("Executing flight procedures query: {}", query);
        
        MYSQL_RES* result = db.executeSelectQuery(query);
        if (!result) {
            logger_->warn("Flight procedures query failed, returning empty list");
            return procedures;
        }
        
        MYSQL_ROW row;
        while ((row = mysql_fetch_row(result))) {
            FlightProcedure procedure;
            
            // STEP 2: Update the mapping logic to read the new columns
            int col = 0;
            procedure.id = row[col] ? std::atoi(row[col]) : 0; col++;
            procedure.procedure_code = row[col] ? std::string(row[col]) : ""; col++;
            procedure.name = row[col] ? std::string(row[col]) : ""; col++;
            procedure.type = row[col] ? stringToProcedureType(std::string(row[col])) : ProcedureType::SID; col++;
            procedure.airport_icao = row[col] ? std::string(row[col]) : ""; col++;
            if (row[col]) procedure.runway = std::string(row[col]); col++;
            if (row[col]) procedure.description = std::string(row[col]); col++;
            
            // This is the new mapping part
            if (row[col]) procedure.trajectory_geometry = std::string(row[col]); col++;
            if (row[col]) procedure.protection_geometry = std::string(row[col]); col++;
            
            // Resume with the rest of the columns
            if (row[col]) procedure.effective_date = stringToTimePoint(std::string(row[col])); col++;
            if (row[col]) procedure.expiry_date = stringToTimePoint(std::string(row[col])); col++;
            procedure.is_active = row[col] ? (std::atoi(row[col]) != 0) : true; col++;
            if (row[col]) procedure.created_at = stringToTimePoint(std::string(row[col])); col++;
            if (row[col]) procedure.updated_at = stringToTimePoint(std::string(row[col])); col++;
            
            procedures.push_back(procedure);
        }
        
        mysql_free_result(result);
        
        logger_->debug("Found {} flight procedures", procedures.size());
    } catch (const std::exception& err) {
        logger_->error("Failed to fetch flight procedures: {}", err.what());
        return std::vector<FlightProcedure>();
    }
    
    return procedures;
}


std::optional<FlightProcedure> FlightProcedureRepository::findById(int id) {
    try {
        auto& db = DatabaseManager::getInstance();
        
        std::stringstream query;
        query << buildSelectQuery() << " WHERE fp.id = " << id;
        
        MYSQL_RES* result = db.executeSelectQuery(query.str());
        if (!result) {
            throw std::runtime_error("Failed to execute query");
        }
        
        MYSQL_ROW row = mysql_fetch_row(result);
        if (row) {
            unsigned long* lengths = mysql_fetch_lengths(result);
            FlightProcedure procedure = rowToProcedure(row, lengths);
            mysql_free_result(result);
            
            // Load related data
            loadRelatedData(procedure, true, true);
            
            return procedure;
        }
        
        mysql_free_result(result);
    } catch (const std::exception& err) {
        logger_->error("Failed to find flight procedure by id {}: {}", id, err.what());
        throw;
    }
    
    return std::nullopt;
}

std::optional<FlightProcedure> FlightProcedureRepository::findByCode(const std::string& code) {
    try {
        auto& db = DatabaseManager::getInstance();
        
        std::stringstream query;
        query << buildSelectQuery() << " WHERE fp.procedure_code = '" << code << "'";
        
        MYSQL_RES* result = db.executeSelectQuery(query.str());
        if (!result) {
            throw std::runtime_error("Failed to execute query");
        }
        
        MYSQL_ROW row = mysql_fetch_row(result);
        if (row) {
            unsigned long* lengths = mysql_fetch_lengths(result);
            FlightProcedure procedure = rowToProcedure(row, lengths);
            mysql_free_result(result);
            
            // Load related data
            loadRelatedData(procedure, true, true);
            
            return procedure;
        }
        
        mysql_free_result(result);
    } catch (const std::exception& err) {
        logger_->error("Failed to find flight procedure by code {}: {}", code, err.what());
        throw;
    }
    
    return std::nullopt;
}

std::vector<FlightProcedure> FlightProcedureRepository::findByAirport(const std::string& airport_icao) {
    FlightProcedureFilter filter;
    filter.airport_icao = airport_icao;
    filter.is_active = true;
    return findAll(filter);
}

std::vector<ProcedureSegment> FlightProcedureRepository::getSegments(int procedure_id) {
    std::vector<ProcedureSegment> segments;
    
    try {
        auto& db = DatabaseManager::getInstance();
        
        // Check if procedure_segments table exists
        MYSQL_RES* tableCheck = db.executeSelectQuery("SHOW TABLES LIKE 'procedure_segments'");
        if (!tableCheck) {
            logger_->warn("Cannot check if procedure_segments table exists");
            return segments;
        }
        
        MYSQL_ROW tableRow = mysql_fetch_row(tableCheck);
        if (!tableRow) {
            logger_->warn("procedure_segments table does not exist");
            mysql_free_result(tableCheck);
            return segments;
        }
        mysql_free_result(tableCheck);
        
        std::stringstream query;
        query << buildSegmentSelectQuery() << " WHERE ps.procedure_id = " << procedure_id;
        query << " ORDER BY ps.segment_order ASC";
        
        MYSQL_RES* result = db.executeSelectQuery(query.str());
        if (!result) {
            logger_->warn("Segments query failed for procedure {}", procedure_id);
            return segments;
        }
        
        MYSQL_ROW row;
        while ((row = mysql_fetch_row(result))) {
            unsigned long* lengths = mysql_fetch_lengths(result);
            segments.push_back(rowToSegment(row, lengths));
        }
        
        mysql_free_result(result);
        
        logger_->debug("Found {} segments for procedure {}", segments.size(), procedure_id);
    } catch (const std::exception& err) {
        logger_->error("Failed to fetch segments for procedure {}: {}", procedure_id, err.what());
        // Return empty list instead of throwing
        return std::vector<ProcedureSegment>();
    }
    
    return segments;
}

std::vector<ProcedureProtection> FlightProcedureRepository::getProtections(int procedure_id) {
    std::vector<ProcedureProtection> protections;
    
    try {
        auto& db = DatabaseManager::getInstance();
        
        // Check if flight_procedure_protection table exists
        MYSQL_RES* tableCheck = db.executeSelectQuery("SHOW TABLES LIKE 'flight_procedure_protection'");
        if (!tableCheck) {
            logger_->warn("Cannot check if flight_procedure_protection table exists");
            return protections;
        }
        
        MYSQL_ROW tableRow = mysql_fetch_row(tableCheck);
        if (!tableRow) {
            logger_->warn("flight_procedure_protection table does not exist");
            mysql_free_result(tableCheck);
            return protections;
        }
        mysql_free_result(tableCheck);
        
        std::stringstream query;
        query << buildProtectionSelectQuery() << " WHERE fpp.procedure_id = " << procedure_id;
        query << " AND fpp.is_active = 1";
        query << " ORDER BY fpp.analysis_priority DESC, fpp.protection_name ASC";
        
        MYSQL_RES* result = db.executeSelectQuery(query.str());
        if (!result) {
            logger_->warn("Protections query failed for procedure {}", procedure_id);
            return protections;
        }
        
        MYSQL_ROW row;
        while ((row = mysql_fetch_row(result))) {
            unsigned long* lengths = mysql_fetch_lengths(result);
            protections.push_back(rowToProtection(row, lengths));
        }
        
        mysql_free_result(result);
        
        logger_->debug("Found {} protections for procedure {}", protections.size(), procedure_id);
    } catch (const std::exception& err) {
        logger_->error("Failed to fetch protections for procedure {}: {}", procedure_id, err.what());
        // Return empty list instead of throwing
        return std::vector<ProcedureProtection>();
    }
    
    return protections;
}

// int FlightProcedureRepository::count(const FlightProcedureFilter& filter) {
//     try {
//         auto& db = DatabaseManager::getInstance();
        
//         std::stringstream query;
//         query << buildCountQuery() << " WHERE 1=1";
        
//         if (filter.type) {
//             query << " AND type = '" << procedureTypeToString(*filter.type) << "'";
//         }
        
//         if (filter.airport_icao) {
//             query << " AND airport_icao = '" << *filter.airport_icao << "'";
//         }
        
//         if (filter.runway) {
//             query << " AND runway = '" << *filter.runway << "'";
//         }
        
//         if (filter.is_active) {
//             query << " AND is_active = " << (*filter.is_active ? 1 : 0);
//         }
        
//         MYSQL_RES* result = db.executeSelectQuery(query.str());
//         if (!result) {
//             throw std::runtime_error("Failed to execute count query");
//         }
        
//         MYSQL_ROW row = mysql_fetch_row(result);
//         int count = 0;
//         if (row && row[0]) {
//             count = std::atoi(row[0]);
//         }
        
//         mysql_free_result(result);
//         return count;
        
//     } catch (const std::exception& err) {
//         logger_->error("Failed to count flight procedures: {}", err.what());
//         throw;
//     }
// }

int FlightProcedureRepository::count(const FlightProcedureFilter& filter) {
    try {
        auto& db = DatabaseManager::getInstance();
        
        std::stringstream query;
        query << "SELECT COUNT(*) FROM flight_procedures fp WHERE 1=1";
        
        if (filter.type) {
            query << " AND fp.type = '" << procedureTypeToString(*filter.type) << "'";
        }
        
        if (filter.airport_icao) {
            query << " AND fp.airport_icao = '" << *filter.airport_icao << "'";
        }
        
        if (filter.runway) {
            query << " AND fp.runway = '" << *filter.runway << "'";
        }
        
        if (filter.is_active) {
            query << " AND fp.is_active = " << (*filter.is_active ? 1 : 0);
        }
        
        logger_->debug("Executing count query: {}", query.str());
        
        MYSQL_RES* result = db.executeSelectQuery(query.str());
        if (!result) {
            logger_->warn("Count query failed, returning 0");
            return 0;
        }
        
        MYSQL_ROW row = mysql_fetch_row(result);
        int count = 0;
        if (row && row[0]) {
            count = std::atoi(row[0]);
            logger_->debug("Count query returned: {}", count);
        }
        
        mysql_free_result(result);
        return count;
        
    } catch (const std::exception& err) {
        logger_->error("Failed to count flight procedures: {}", err.what());
        return 0;
    }
}

void FlightProcedureRepository::loadRelatedData(FlightProcedure& procedure, bool include_segments, bool include_protections) {
    // if (include_segments) {
    //     procedure.segments = getSegments(procedure.id);
    // }
    
    // if (include_protections) {
    //     procedure.protections = getProtections(procedure.id);
    // }
}

FlightProcedure FlightProcedureRepository::rowToProcedure(MYSQL_ROW row, unsigned long* lengths) {
    FlightProcedure p;
    
    int col = 0;
    p.id = row[col] ? std::atoi(row[col]) : 0; col++;
    p.procedure_code = row[col] ? std::string(row[col]) : ""; col++;
    p.name = row[col] ? std::string(row[col]) : ""; col++;
    p.type = row[col] ? stringToProcedureType(std::string(row[col])) : ProcedureType::SID; col++;
    p.airport_icao = row[col] ? std::string(row[col]) : ""; col++;
    if (row[col]) p.runway = std::string(row[col]); col++;
    if (row[col]) p.description = std::string(row[col]); col++;
    if (row[col]) p.effective_date = stringToTimePoint(std::string(row[col])); col++;
    if (row[col]) p.expiry_date = stringToTimePoint(std::string(row[col])); col++;
    p.is_active = row[col] ? (std::atoi(row[col]) != 0) : true; col++;
    if (row[col]) p.created_at = stringToTimePoint(std::string(row[col])); col++;
    if (row[col]) p.updated_at = stringToTimePoint(std::string(row[col])); col++;
    
    return p;
}

ProcedureSegment FlightProcedureRepository::rowToSegment(MYSQL_ROW row, unsigned long* lengths) {
    ProcedureSegment s;
    
    int col = 0;
    s.id = row[col] ? std::atoi(row[col]) : 0; col++;
    s.procedure_id = row[col] ? std::atoi(row[col]) : 0; col++;
    s.segment_order = row[col] ? std::atoi(row[col]) : 0; col++;
    if (row[col]) s.segment_name = std::string(row[col]); col++;
    if (row[col]) s.waypoint_from = std::string(row[col]); col++;
    if (row[col]) s.waypoint_to = std::string(row[col]); col++;
    if (row[col]) s.altitude_min = std::atoi(row[col]); col++;
    if (row[col]) s.altitude_max = std::atoi(row[col]); col++;
    if (row[col]) s.altitude_restriction = stringToAltitudeRestriction(std::string(row[col])); col++;
    if (row[col]) s.speed_limit = std::atoi(row[col]); col++;
    if (row[col]) s.speed_restriction = stringToSpeedRestriction(std::string(row[col])); col++;
    s.trajectory_geometry = row[col] ? std::string(row[col]) : ""; col++;
    if (row[col]) s.segment_length = std::atof(row[col]); col++;
    if (row[col]) s.magnetic_course = std::atoi(row[col]); col++;
    s.turn_direction = row[col] ? stringToTurnDirection(std::string(row[col])) : TurnDirection::Straight; col++;
    s.is_mandatory = row[col] ? (std::atoi(row[col]) != 0) : true; col++;
    
    return s;
}

ProcedureProtection FlightProcedureRepository::rowToProtection(MYSQL_ROW row, unsigned long* lengths) {
    ProcedureProtection p;
    
    int col = 0;
    p.id = row[col] ? std::atoi(row[col]) : 0; col++;
    p.procedure_id = row[col] ? std::atoi(row[col]) : 0; col++;
    p.protection_name = row[col] ? std::string(row[col]) : ""; col++;
    p.protection_type = row[col] ? stringToProtectionType(std::string(row[col])) : ProtectionType::OverallPrimary; col++;
    if (row[col]) p.description = std::string(row[col]); col++;
    p.protection_geometry = row[col] ? std::string(row[col]) : ""; col++;
    if (row[col]) p.altitude_min = std::atoi(row[col]); col++;
    if (row[col]) p.altitude_max = std::atoi(row[col]); col++;
    p.altitude_reference = row[col] ? stringToAltitudeReference(std::string(row[col])) : AltitudeReference::MSL; col++;
    if (row[col]) p.area_size = std::atof(row[col]); col++;
    if (row[col]) p.center_lat = std::atof(row[col]); col++;
    if (row[col]) p.center_lng = std::atof(row[col]); col++;
    if (row[col]) p.buffer_distance = std::atof(row[col]); col++;
    p.restriction_level = row[col] ? stringToRestrictionLevel(std::string(row[col])) : RestrictionLevel::Restricted; col++;
    p.conflict_severity = row[col] ? stringToConflictSeverity(std::string(row[col])) : ConflictSeverity::Medium; col++;
    p.analysis_priority = row[col] ? std::atoi(row[col]) : 50; col++;
    if (row[col]) p.time_restriction = std::string(row[col]); col++;
    p.weather_dependent = row[col] ? (std::atoi(row[col]) != 0) : false; col++;
    if (row[col]) p.regulatory_source = std::string(row[col]); col++;
    if (row[col]) p.operational_notes = std::string(row[col]); col++;
    if (row[col]) p.contact_info = std::string(row[col]); col++;
    p.is_active = row[col] ? (std::atoi(row[col]) != 0) : true; col++;
    if (row[col]) p.effective_date = stringToTimePoint(std::string(row[col])); col++;
    if (row[col]) p.expiry_date = stringToTimePoint(std::string(row[col])); col++;
    if (row[col]) p.review_date = stringToTimePoint(std::string(row[col])); col++;
    if (row[col]) p.created_at = stringToTimePoint(std::string(row[col])); col++;
    if (row[col]) p.updated_at = stringToTimePoint(std::string(row[col])); col++;
    if (row[col]) p.created_by = std::atoi(row[col]); col++;
    if (row[col]) p.last_reviewed_by = std::atoi(row[col]); col++;
    if (row[col]) p.last_review_date = stringToTimePoint(std::string(row[col])); col++;
    
    return p;
}

std::vector<ProcedureProtection> FlightProcedureRepository::findAllActiveProtections() {
    std::vector<ProcedureProtection> protections;
    try {
        auto& db = DatabaseManager::getInstance();
        
        // Query the flight_procedures table directly since protection_geometry is stored there
        std::string query = "SELECT id, procedure_code, name, type, airport_icao, "
                           "runway, description, protection_geometry, "
                           "effective_date, expiry_date, is_active "
                           "FROM flight_procedures fp "
                           "WHERE fp.is_active = 1 AND fp.protection_geometry IS NOT NULL "
                           "AND fp.protection_geometry != ''";
        
        logger_->debug("Executing active protections query: {}", query);
        
        MYSQL_RES* result = db.executeSelectQuery(query);
        if (result) {
            MYSQL_ROW row;
            while ((row = mysql_fetch_row(result))) {
                // Convert flight_procedures row to ProcedureProtection
                ProcedureProtection protection;
                
                int col = 0;
                protection.procedure_id = row[col] ? std::atoi(row[col]) : 0; col++; // id -> procedure_id
                std::string procedure_code = row[col] ? std::string(row[col]) : ""; col++; // procedure_code
                std::string procedure_name = row[col] ? std::string(row[col]) : ""; col++; // name
                std::string procedure_type = row[col] ? std::string(row[col]) : ""; col++; // type
                std::string airport_icao = row[col] ? std::string(row[col]) : ""; col++; // airport_icao
                std::string runway = row[col] ? std::string(row[col]) : ""; col++; // runway (optional)
                std::string description = row[col] ? std::string(row[col]) : ""; col++; // description (optional)
                
                // The protection_geometry field - this is what we need!
                std::string protection_geometry_str = row[col] ? std::string(row[col]) : "{}"; col++;

                try {
                    nlohmann::json feature_collection = nlohmann::json::parse(protection_geometry_str);
                    
                    if (feature_collection.contains("features") && feature_collection["features"].is_array()) {
                        nlohmann::json multi_polygon_coords = nlohmann::json::array();
                        
                        for (const auto& feature : feature_collection["features"]) {
                            if (feature.contains("geometry") && feature["geometry"]["type"] == "Polygon") {
                                multi_polygon_coords.push_back(feature["geometry"]["coordinates"]);
                            }
                        }
                        
                        nlohmann::json multi_polygon_geom = {
                            {"type", "MultiPolygon"},
                            {"coordinates", multi_polygon_coords}
                        };
                        
                        protection.protection_geometry = multi_polygon_geom.dump();
                    } else {
                        // If it's not a FeatureCollection, use it as is (or handle as an error)
                        protection.protection_geometry = protection_geometry_str;
                    }
                } catch (const std::exception& e) {
                    logger_->warn("Could not parse protection geometry for procedure {}: {}", protection.procedure_id, e.what());
                    protection.protection_geometry = "{}"; // Assign empty geometry
                }

                // Set default values for protection-specific fields
                protection.id = protection.procedure_id; // Use same ID
                protection.protection_name = procedure_code + " - " + procedure_name + " Protection Zone";
                protection.protection_type = ProtectionType::OverallPrimary; // Default type
                protection.description = description.empty() ? 
                    ("Protection zone for " + procedure_type + " procedure at " + airport_icao) : 
                    description;
                
                // Default values for other fields
                protection.altitude_reference = AltitudeReference::MSL;
                protection.restriction_level = RestrictionLevel::Restricted;
                protection.conflict_severity = ConflictSeverity::High; // Flight procedures are high priority
                protection.analysis_priority = 80; // High priority for flight procedures
                protection.weather_dependent = false;
                protection.is_active = true;
                protection.created_at = std::chrono::system_clock::now();
                protection.updated_at = std::chrono::system_clock::now();
                
                // Skip the remaining columns (effective_date, expiry_date, is_active) for now
                
                protections.push_back(protection);
            }
            mysql_free_result(result);
        } else {
            logger_->warn("Failed to execute active protections query");
        }
    } catch (const std::exception& err) {
        logger_->error("Failed to find all active protections: {}", err.what());
    }
    
    logger_->info("Found {} active flight procedure protections for analysis.", protections.size());
    return protections;
}

// Create, Update, Delete operations (simplified for brevity)
FlightProcedure FlightProcedureRepository::create(const FlightProcedure& procedure) {
    // Implementation would go here
    throw std::runtime_error("Create operation not implemented yet");
}

bool FlightProcedureRepository::update(int id, const FlightProcedure& procedure) {
    // Implementation would go here
    throw std::runtime_error("Update operation not implemented yet");
}

bool FlightProcedureRepository::deleteById(int id) {
    // Implementation would go here
    throw std::runtime_error("Delete operation not implemented yet");
}

ProcedureSegment FlightProcedureRepository::createSegment(const ProcedureSegment& segment) {
    // Implementation would go here
    throw std::runtime_error("Create segment operation not implemented yet");
}

bool FlightProcedureRepository::updateSegment(int id, const ProcedureSegment& segment) {
    // Implementation would go here
    throw std::runtime_error("Update segment operation not implemented yet");
}

bool FlightProcedureRepository::deleteSegment(int id) {
    // Implementation would go here
    throw std::runtime_error("Delete segment operation not implemented yet");
}

ProcedureProtection FlightProcedureRepository::createProtection(const ProcedureProtection& protection) {
    // Implementation would go here
    throw std::runtime_error("Create protection operation not implemented yet");
}

bool FlightProcedureRepository::updateProtection(int id, const ProcedureProtection& protection) {
    // Implementation would go here
    throw std::runtime_error("Update protection operation not implemented yet");
}

bool FlightProcedureRepository::deleteProtection(int id) {
    // Implementation would go here
    throw std::runtime_error("Delete protection operation not implemented yet");
}

#else
// MySQL Connector/C++ implementation (original code would go here)
// For now, throw an error since we're using MySQL C API
#error "MySQL Connector/C++ code not implemented in this version"
#endif

} // namespace aeronautical