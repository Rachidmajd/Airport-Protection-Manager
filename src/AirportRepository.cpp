#include "AirportRepository.h"
#include "DatabaseManager.h"
#include <mysql/mysql.h>
#include <sstream>
#include <stdexcept>

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

// Helper to populate an Airport struct from a MYSQL_ROW
static void populateAirportFromRow(Airport& airport, MYSQL_ROW row) {
    airport.id = row[0] ? std::stoi(row[0]) : 0;
    airport.icao_code = row[1] ? row[1] : "";
    airport.iata_code = row[2] ? row[2] : "";
    airport.name = row[3] ? row[3] : "";
    airport.full_name = row[4] ? row[4] : "";
    airport.latitude = row[5] ? std::stod(row[5]) : 0.0;
    airport.longitude = row[6] ? std::stod(row[6]) : 0.0;
    airport.elevation_ft = row[7] ? std::stoi(row[7]) : 0;
    airport.airport_type = row[8] ? row[8] : "";
    airport.municipality = row[9] ? row[9] : "";
    airport.region = row[10] ? row[10] : "";
    airport.country_code = row[11] ? row[11] : "";
    airport.country_name = row[12] ? row[12] : "";
    airport.is_active = row[14] ? (std::string(row[14]) == "1") : false;
    airport.has_tower = row[15] ? (std::string(row[15]) == "1") : false;
    airport.has_ils = row[16] ? (std::string(row[16]) == "1") : false;
    airport.runway_count = row[17] ? std::stoi(row[17]) : 0;
    airport.longest_runway_ft = row[18] ? std::stoi(row[18]) : 0;
}

AirportRepository::AirportRepository() {}

std::vector<Airport> AirportRepository::fetchAllAirports(const std::string& filter_type, bool active_only) {
    std::vector<Airport> airports;
    MYSQL* con = DatabaseManager::getInstance().getConnection();
    std::stringstream ss;
    ss << "SELECT * FROM airports WHERE 1=1";
    if (active_only) {
        ss << " AND is_active = TRUE";
    }
    if (!filter_type.empty()) {
        ss << " AND airport_type = '" << escapeString(con, filter_type) << "'";
    }

    if (mysql_query(con, ss.str().c_str())) {
        // In a real application, you'd log this error
        return airports;
    }

    MYSQL_RES* result = mysql_store_result(con);
    if (result == NULL) {
        return airports;
    }

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result))) {
        Airport airport;
        populateAirportFromRow(airport, row);
        airports.push_back(airport);
    }
    mysql_free_result(result);
    return airports;
}

Airport AirportRepository::fetchAirportByIcao(const std::string& icao_code) {
    MYSQL* con = DatabaseManager::getInstance().getConnection();
    std::stringstream ss;
    ss << "SELECT * FROM airports WHERE icao_code = '" << escapeString(con, icao_code) << "' LIMIT 1";

    if (mysql_query(con, ss.str().c_str())) {
        throw std::runtime_error("Database query failed");
    }

    MYSQL_RES* result = mysql_store_result(con);
    if (result == NULL || mysql_num_rows(result) == 0) {
        if(result) mysql_free_result(result);
        throw std::runtime_error("Airport not found");
    }

    MYSQL_ROW row = mysql_fetch_row(result);
    Airport airport;
    populateAirportFromRow(airport, row);
    
    mysql_free_result(result);
    return airport;
}

std::vector<Airport> AirportRepository::fetchAirportsByCountry(const std::string& country_code, bool active_only) {
    std::vector<Airport> airports;
    MYSQL* con = DatabaseManager::getInstance().getConnection();
    std::stringstream ss;
    ss << "SELECT * FROM airports WHERE country_code = '" << escapeString(con, country_code) << "'";
    if (active_only) {
        ss << " AND is_active = TRUE";
    }
    
    if (mysql_query(con, ss.str().c_str())) return airports;

    MYSQL_RES* result = mysql_store_result(con);
    if (result == NULL) return airports;

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result))) {
        Airport airport;
        populateAirportFromRow(airport, row);
        airports.push_back(airport);
    }
    mysql_free_result(result);
    return airports;
}

std::vector<Airport> AirportRepository::fetchAirportsInBounds(double min_lat, double max_lat, double min_lng, double max_lng, const std::string& filter_type) {
    std::vector<Airport> airports;
    MYSQL* con = DatabaseManager::getInstance().getConnection();
    std::stringstream ss;
    ss << "SELECT * FROM airports WHERE (latitude BETWEEN " << min_lat << " AND " << max_lat 
       << ") AND (longitude BETWEEN " << min_lng << " AND " << max_lng << ")";
    if (!filter_type.empty()) {
        ss << " AND airport_type = '" << escapeString(con, filter_type) << "'";
    }
    ss << " AND is_active = TRUE";
    
    if (mysql_query(con, ss.str().c_str())) return airports;
    
    MYSQL_RES* result = mysql_store_result(con);
    if (result == NULL) return airports;

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result))) {
        Airport airport;
        populateAirportFromRow(airport, row);
        airports.push_back(airport);
    }
    mysql_free_result(result);
    return airports;
}

std::vector<Airport> AirportRepository::searchAirportsByQuery(const std::string& query, int limit) {
    std::vector<Airport> airports;
    MYSQL* con = DatabaseManager::getInstance().getConnection();
    std::string search_query = "%" + escapeString(con, query) + "%";
    std::stringstream ss;
    ss << "SELECT * FROM airports WHERE (name LIKE '" << search_query 
        << "' OR icao_code LIKE '" << search_query 
        << "' OR iata_code LIKE '" << search_query 
        << "' OR municipality LIKE '" << search_query 
        << "') AND is_active = TRUE LIMIT " << limit;

    if (mysql_query(con, ss.str().c_str())) return airports;
    
    MYSQL_RES* result = mysql_store_result(con);
    if (result == NULL) return airports;

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result))) {
        Airport airport;
        populateAirportFromRow(airport, row);
        airports.push_back(airport);
    }
    mysql_free_result(result);
    return airports;
}

} // namespace aeronautical