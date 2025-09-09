#include "WaypointRepository.h"
#include "DatabaseManager.h"
#include <mysql/mysql.h>
#include <sstream>
#include <stdexcept>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace aeronautical {

// Helper function to escape strings using the C API
static std::string escapeString(MYSQL* con, const std::string& str) {
    if (str.empty()) {
        return "";
    }
    char* escaped_str = new char[str.length() * 2 + 1];
    mysql_real_escape_string(con, escaped_str, str.c_str(), str.length());
    std::string result(escaped_str);
    delete[] escaped_str;
    return result;
}

// Helper to populate a Waypoint struct from a MYSQL_ROW
static void populateWaypointFromRow(Waypoint& waypoint, MYSQL_ROW row) {
    waypoint.id = row[0] ? std::stoi(row[0]) : 0;
    waypoint.waypoint_code = row[1] ? row[1] : "";
    waypoint.name = row[2] ? row[2] : "";
    waypoint.latitude = row[3] ? std::stod(row[3]) : 0.0;
    waypoint.longitude = row[4] ? std::stod(row[4]) : 0.0;
    waypoint.elevation_ft = row[5] ? std::stoi(row[5]) : 0;
    waypoint.waypoint_type = row[6] ? row[6] : "";
    waypoint.country_code = row[7] ? row[7] : "";
    waypoint.country_name = row[8] ? row[8] : "";
    waypoint.region = row[9] ? row[9] : "";
    waypoint.frequency = row[10] ? row[10] : "";
    waypoint.usage_type = row[11] ? row[11] : "";
    waypoint.is_active = row[12] ? (std::string(row[12]) == "1") : false;
}

WaypointRepository::WaypointRepository() {
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

std::vector<Waypoint> WaypointRepository::fetchAllWaypoints(const std::string& filter_type, bool active_only) {
    std::vector<Waypoint> waypoints;
    MYSQL* con = DatabaseManager::getInstance().getConnection();
    std::stringstream ss;
    
    // Base query - adjust column names based on your actual table structure
    ss << "SELECT id, waypoint_code, name, latitude, longitude, elevation_ft, "
       << "waypoint_type, country_code, country_name, region, frequency, usage_type, is_active "
       << "FROM waypoints WHERE 1=1";
       
    if (active_only) {
        ss << " AND is_active = TRUE";
    }
    if (!filter_type.empty()) {
        ss << " AND waypoint_type = '" << escapeString(con, filter_type) << "'";
    }
    ss << " ORDER BY waypoint_code ASC";

    if (mysql_query(con, ss.str().c_str())) {
        logger_->error("Waypoints query failed: {}", mysql_error(con));
        return waypoints;
    }

    MYSQL_RES* result = mysql_store_result(con);
    if (result == NULL) {
        logger_->warn("No waypoints result set returned");
        return waypoints;
    }

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result))) {
        Waypoint waypoint;
        populateWaypointFromRow(waypoint, row);
        waypoints.push_back(waypoint);
    }
    mysql_free_result(result);
    
    logger_->debug("Found {} waypoints", waypoints.size());
    return waypoints;
}

std::optional<Waypoint> WaypointRepository::fetchWaypointByCode(const std::string& waypoint_code) {
    MYSQL* con = DatabaseManager::getInstance().getConnection();
    std::stringstream ss;
    ss << "SELECT id, waypoint_code, name, latitude, longitude, elevation_ft, "
       << "waypoint_type, country_code, country_name, region, frequency, usage_type, is_active "
       << "FROM waypoints WHERE waypoint_code = '" << escapeString(con, waypoint_code) << "' LIMIT 1";

    if (mysql_query(con, ss.str().c_str())) {
        logger_->error("Waypoint by code query failed: {}", mysql_error(con));
        return std::nullopt;
    }

    MYSQL_RES* result = mysql_store_result(con);
    if (result == NULL || mysql_num_rows(result) == 0) {
        if(result) mysql_free_result(result);
        return std::nullopt;
    }

    MYSQL_ROW row = mysql_fetch_row(result);
    Waypoint waypoint;
    populateWaypointFromRow(waypoint, row);
    
    mysql_free_result(result);
    return waypoint;
}

std::vector<Waypoint> WaypointRepository::fetchWaypointsByCountry(const std::string& country_code, bool active_only) {
    std::vector<Waypoint> waypoints;
    MYSQL* con = DatabaseManager::getInstance().getConnection();
    std::stringstream ss;
    ss << "SELECT id, waypoint_code, name, latitude, longitude, elevation_ft, "
       << "waypoint_type, country_code, country_name, region, frequency, usage_type, is_active "
       << "FROM waypoints WHERE country_code = '" << escapeString(con, country_code) << "'";
    if (active_only) {
        ss << " AND is_active = TRUE";
    }
    ss << " ORDER BY waypoint_code ASC";
    
    if (mysql_query(con, ss.str().c_str())) {
        logger_->error("Waypoints by country query failed: {}", mysql_error(con));
        return waypoints;
    }

    MYSQL_RES* result = mysql_store_result(con);
    if (result == NULL) return waypoints;

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result))) {
        Waypoint waypoint;
        populateWaypointFromRow(waypoint, row);
        waypoints.push_back(waypoint);
    }
    mysql_free_result(result);
    
    logger_->debug("Found {} waypoints for country {}", waypoints.size(), country_code);
    return waypoints;
}

std::vector<Waypoint> WaypointRepository::fetchWaypointsInBounds(double min_lat, double max_lat, 
                                                                double min_lng, double max_lng, 
                                                                const std::string& filter_type) {
    std::vector<Waypoint> waypoints;
    MYSQL* con = DatabaseManager::getInstance().getConnection();
    std::stringstream ss;
    ss << "SELECT id, waypoint_code, name, latitude, longitude, elevation_ft, "
       << "waypoint_type, country_code, country_name, region, frequency, usage_type, is_active "
       << "FROM waypoints WHERE (latitude BETWEEN " << min_lat << " AND " << max_lat 
       << ") AND (longitude BETWEEN " << min_lng << " AND " << max_lng << ")";
    if (!filter_type.empty()) {
        ss << " AND waypoint_type = '" << escapeString(con, filter_type) << "'";
    }
    ss << " AND is_active = TRUE ORDER BY waypoint_code ASC";
    
    if (mysql_query(con, ss.str().c_str())) {
        logger_->error("Waypoints in bounds query failed: {}", mysql_error(con));
        return waypoints;
    }
    
    MYSQL_RES* result = mysql_store_result(con);
    if (result == NULL) return waypoints;

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result))) {
        Waypoint waypoint;
        populateWaypointFromRow(waypoint, row);
        waypoints.push_back(waypoint);
    }
    mysql_free_result(result);
    
    logger_->debug("Found {} waypoints in bounds", waypoints.size());
    return waypoints;
}

std::vector<Waypoint> WaypointRepository::searchWaypointsByQuery(const std::string& query, int limit) {
    std::vector<Waypoint> waypoints;
    MYSQL* con = DatabaseManager::getInstance().getConnection();
    std::string search_query = "%" + escapeString(con, query) + "%";
    std::stringstream ss;
    ss << "SELECT id, waypoint_code, name, latitude, longitude, elevation_ft, "
       << "waypoint_type, country_code, country_name, region, frequency, usage_type, is_active "
       << "FROM waypoints WHERE (name LIKE '" << search_query 
        << "' OR waypoint_code LIKE '" << search_query 
        << "' OR waypoint_type LIKE '" << search_query 
        << "') AND is_active = TRUE ORDER BY waypoint_code ASC LIMIT " << limit;

    if (mysql_query(con, ss.str().c_str())) {
        logger_->error("Waypoints search query failed: {}", mysql_error(con));
        return waypoints;
    }
    
    MYSQL_RES* result = mysql_store_result(con);
    if (result == NULL) return waypoints;

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result))) {
        Waypoint waypoint;
        populateWaypointFromRow(waypoint, row);
        waypoints.push_back(waypoint);
    }
    mysql_free_result(result);
    
    logger_->debug("Found {} waypoints for search '{}'", waypoints.size(), query);
    return waypoints;
}

std::vector<Waypoint> WaypointRepository::fetchWaypointsByType(const std::string& waypoint_type, bool active_only) {
    std::vector<Waypoint> waypoints;
    MYSQL* con = DatabaseManager::getInstance().getConnection();
    std::stringstream ss;
    ss << "SELECT id, waypoint_code, name, latitude, longitude, elevation_ft, "
       << "waypoint_type, country_code, country_name, region, frequency, usage_type, is_active "
       << "FROM waypoints WHERE waypoint_type = '" << escapeString(con, waypoint_type) << "'";
    if (active_only) {
        ss << " AND is_active = TRUE";
    }
    ss << " ORDER BY waypoint_code ASC";
    
    if (mysql_query(con, ss.str().c_str())) {
        logger_->error("Waypoints by type query failed: {}", mysql_error(con));
        return waypoints;
    }

    MYSQL_RES* result = mysql_store_result(con);
    if (result == NULL) return waypoints;

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result))) {
        Waypoint waypoint;
        populateWaypointFromRow(waypoint, row);
        waypoints.push_back(waypoint);
    }
    mysql_free_result(result);
    
    logger_->debug("Found {} waypoints of type {}", waypoints.size(), waypoint_type);
    return waypoints;
}

std::vector<Waypoint> WaypointRepository::fetchWaypointsByUsage(const std::string& usage_type, bool active_only) {
    std::vector<Waypoint> waypoints;
    MYSQL* con = DatabaseManager::getInstance().getConnection();
    std::stringstream ss;
    ss << "SELECT id, waypoint_code, name, latitude, longitude, elevation_ft, "
       << "waypoint_type, country_code, country_name, region, frequency, usage_type, is_active "
       << "FROM waypoints WHERE usage_type = '" << escapeString(con, usage_type) << "'";
    if (active_only) {
        ss << " AND is_active = TRUE";
    }
    ss << " ORDER BY waypoint_code ASC";
    
    if (mysql_query(con, ss.str().c_str())) {
        logger_->error("Waypoints by usage query failed: {}", mysql_error(con));
        return waypoints;
    }

    MYSQL_RES* result = mysql_store_result(con);
    if (result == NULL) return waypoints;

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result))) {
        Waypoint waypoint;
        populateWaypointFromRow(waypoint, row);
        waypoints.push_back(waypoint);
    }
    mysql_free_result(result);
    
    logger_->debug("Found {} waypoints for usage {}", waypoints.size(), usage_type);
    return waypoints;
}

} // namespace aeronautical