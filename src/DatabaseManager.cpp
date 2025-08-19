#include "DatabaseManager.h"
#include <chrono>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace aeronautical {

DatabaseManager& DatabaseManager::getInstance() {
    static DatabaseManager instance;
    return instance;
}

DatabaseManager::DatabaseManager() : connection_(nullptr) {
}

DatabaseManager::~DatabaseManager() {
#ifdef USE_MYSQL_C_API
    cleanup();
#else
    if (session_) {
        session_->close();
    }
#endif
}

void DatabaseManager::initialize(const std::string& host, int port, 
                                const std::string& user, const std::string& password, 
                                const std::string& database) {
    try {
        // Initialize logger
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
        
        database_name_ = database;
        
#ifdef USE_MYSQL_C_API
        // Store connection parameters for reconnection
        host_ = host;
        port_ = port;
        user_ = user;
        password_ = password;
        
        // Initialize MySQL library
        if (mysql_library_init(0, nullptr, nullptr)) {
            throw std::runtime_error("Could not initialize MySQL library");
        }
        
        // Create initial connection
        connectToDatabase();
        
        logger_->info("Database connection established to {}:{}/{} (MySQL C API)", host, port, database);
        
#else
        std::stringstream ss;
        ss << "mysqlx://" << user << ":" << password << "@" 
           << host << ":" << port << "/" << database;
        
        session_ = std::make_unique<mysqlx::Session>(ss.str());
        
        logger_->info("Database connection established to {}:{}/{} (MySQL Connector/C++)", host, port, database);
#endif
    } catch (const std::exception& err) {
        logger_->error("Failed to connect to database: {}", err.what());
        throw;
    }
}

#ifdef USE_MYSQL_C_API

void DatabaseManager::connectToDatabase() {
    // Clean up any existing connection
    if (connection_) {
        mysql_close(connection_);
        connection_ = nullptr;
    }
    
    // Create new connection
    connection_ = mysql_init(nullptr);
    if (!connection_) {
        throw std::runtime_error("Failed to initialize MySQL connection");
    }
    
    // Set connection options
    unsigned int timeout = 10;
    mysql_options(connection_, MYSQL_OPT_CONNECT_TIMEOUT, &timeout);
    
    // Don't use auto-reconnect as it can cause issues
    my_bool reconnect = 0;
    mysql_options(connection_, MYSQL_OPT_RECONNECT, &reconnect);
    
    // Connect to database
    if (!mysql_real_connect(connection_, host_.c_str(), user_.c_str(), 
                           password_.c_str(), database_name_.c_str(), port_, nullptr, 0)) {
        std::string error = mysql_error(connection_);
        unsigned int error_code = mysql_errno(connection_);
        mysql_close(connection_);
        connection_ = nullptr;
        throw std::runtime_error("Failed to connect to database: " + error + " (Code: " + std::to_string(error_code) + ")");
    }
    
    // Set charset to utf8mb4
    if (mysql_set_character_set(connection_, "utf8mb4")) {
        logger_->warn("Failed to set UTF-8 character set: {}", mysql_error(connection_));
    }
}

MYSQL* DatabaseManager::getConnection() {
    ensureConnected();
    return connection_;
}

bool DatabaseManager::executeQuery(const std::string& query) {
    std::lock_guard<std::recursive_mutex> lock(db_mutex_); // <--- Add lock
    ensureConnected();
    
    logger_->debug("Executing query: {}", query);
    
    if (mysql_query(connection_, query.c_str())) {
        unsigned int error_code = mysql_errno(connection_);
        const char* error_msg = mysql_error(connection_);
        const char* sqlstate = mysql_sqlstate(connection_);
        
        logger_->error("Query failed: {} - Error Code: {}, SQLSTATE: {}, Message: '{}'", 
                      query, error_code, sqlstate ? sqlstate : "unknown", 
                      error_msg ? error_msg : "no error message");
        return false;
    }
    
    logger_->debug("Query executed successfully");
    return true;
}

// MYSQL_RES* DatabaseManager::executeSelectQuery(const std::string& query) {
//     ensureConnected();
    
//     logger_->debug("Executing select query: {}", query);
    
//     if (mysql_query(connection_, query.c_str())) {
//         unsigned int error_code = mysql_errno(connection_);
//         const char* error_msg = mysql_error(connection_);
//         const char* sqlstate = mysql_sqlstate(connection_);
        
//         logger_->error("Select query failed: {} - Error Code: {}, SQLSTATE: {}, Message: '{}'", 
//                       query, error_code, sqlstate ? sqlstate : "unknown", 
//                       error_msg ? error_msg : "no error message");
        
//         return nullptr;
//     }
    
//     MYSQL_RES* result = mysql_store_result(connection_);
//     if (!result) {
//         // Check if this was supposed to return a result set
//         if (mysql_field_count(connection_) > 0) {
//             logger_->error("Failed to store result for query: {} - Error: '{}'", 
//                           query, mysql_error(connection_));
//             return nullptr;
//         }
//         // Query didn't return a result set (e.g., INSERT, UPDATE, DELETE)
//         logger_->debug("Query completed without result set");
//     } else {
//         unsigned long num_rows = mysql_num_rows(result);
//         logger_->debug("Query returned {} rows", num_rows);
//     }
    
//     return result;
// }

MYSQL_RES* DatabaseManager::executeSelectQuery(const std::string& query) {
    std::lock_guard<std::recursive_mutex> lock(db_mutex_); // <--- Add lock
    ensureConnected();
    
    logger_->debug("Executing select query: {}", query);
    
    if (mysql_query(connection_, query.c_str())) {
        unsigned int error_code = mysql_errno(connection_);
        const char* error_msg = mysql_error(connection_);
        const char* sqlstate = mysql_sqlstate(connection_);
        
        logger_->error("Select query failed: {} - Error Code: {}, SQLSTATE: {}, Message: '{}'", 
                      query, error_code, sqlstate ? sqlstate : "unknown", 
                      error_msg ? error_msg : "no error message");
        
        return nullptr;
    }
    
    MYSQL_RES* result = mysql_store_result(connection_);
    if (!result) {
        // Check if this was supposed to return a result set
        if (mysql_field_count(connection_) > 0) {
            logger_->error("Failed to store result for query: {} - Error: '{}'", 
                          query, mysql_error(connection_));
            return nullptr;
        } else {
            // Query didn't return a result set (e.g., INSERT, UPDATE, DELETE)
            logger_->debug("Query completed without result set");
            return nullptr;
        }
    } else {
        unsigned long num_rows = mysql_num_rows(result);
        logger_->debug("Query returned {} rows", num_rows);
    }
    
    return result;
}

void DatabaseManager::cleanup() {
    if (connection_) {
        mysql_close(connection_);
        connection_ = nullptr;
    }
    mysql_library_end();
}

#else
mysqlx::Session& DatabaseManager::getSession() {
    ensureConnected();
    return *session_;
}

mysqlx::Schema DatabaseManager::getSchema() {
    ensureConnected();
    return session_->getSchema(database_name_);
}
#endif

bool DatabaseManager::isConnected() {
        std::lock_guard<std::recursive_mutex> lock(db_mutex_); // <--- Add lock

#ifdef USE_MYSQL_C_API

    if (!connection_) {
        return false;
    }
    
    // Use ping instead of a query to test connection
    int ping_result = mysql_ping(connection_);
    if (ping_result != 0) {
        unsigned int error_code = mysql_errno(connection_);
        logger_->debug("Connection ping failed with error code: {}, message: '{}'", 
                      error_code, mysql_error(connection_));
        return false;
    }
    
    return true;
#else
    if (!session_) return false;
    try {
        session_->sql("SELECT 1").execute();
        return true;
    } catch (const mysqlx::Error&) {
        return false;
    }
#endif
}

void DatabaseManager::reconnect() {
        std::lock_guard<std::recursive_mutex> lock(db_mutex_); // <--- Add lock

#ifdef USE_MYSQL_C_API
    logger_->info("Reconnecting to database...");
    connectToDatabase();
#else
    if (session_) {
        session_->close();
        session_.reset();
    }
    throw std::runtime_error("Reconnection not implemented for Connector/C++ version");
#endif
}

void DatabaseManager::ensureConnected() {
        std::lock_guard<std::recursive_mutex> lock(db_mutex_); // <--- Add lock for safety
#ifdef USE_MYSQL_C_API
    if (!connection_) {
        throw std::runtime_error("Database not initialized");
    }
    
    // Test connection and reconnect if needed
    if (!isConnected()) {
        logger_->warn("Database connection lost, attempting to reconnect...");
        try {
            reconnect();
        } catch (const std::exception& e) {
            throw std::runtime_error("Database connection lost and reconnection failed: " + std::string(e.what()));
        }
    }
#else
    if (!session_) {
        throw std::runtime_error("Database not initialized");
    }
    
    try {
        session_->sql("SELECT 1").execute();
    } catch (const mysqlx::Error& err) {
        logger_->warn("Database connection lost, attempting to reconnect...");
        throw std::runtime_error("Database connection lost");
    }
#endif
}

std::string DatabaseManager::generateProjectCode() {
        std::lock_guard<std::recursive_mutex> lock(db_mutex_); // <--- Add lock
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
    ss << "PROJ-" << std::put_time(std::localtime(&time_t), "%Y") << "-";
    
    try {
#ifdef USE_MYSQL_C_API
        std::string query = 
            "SELECT MAX(CAST(SUBSTRING(project_code, -3) AS UNSIGNED)) as max_seq "
            "FROM projects WHERE project_code LIKE CONCAT('PROJ-', YEAR(NOW()), '-%')";
        
        MYSQL_RES* result = executeSelectQuery(query);
        if (!result) {
            throw std::runtime_error("Failed to execute query for project code generation");
        }
        
        int next_seq = 1;
        MYSQL_ROW row = mysql_fetch_row(result);
        if (row && row[0]) {
            next_seq = std::atoi(row[0]) + 1;
        }
        
        mysql_free_result(result);
        ss << std::setfill('0') << std::setw(3) << next_seq;
#else
        auto& session = getSession();
        auto result = session.sql(
            "SELECT MAX(CAST(SUBSTRING(project_code, -3) AS UNSIGNED)) as max_seq "
            "FROM projects WHERE project_code LIKE CONCAT('PROJ-', YEAR(NOW()), '-%')"
        ).execute();
        
        int next_seq = 1;
        if (result.count() > 0) {
            auto row = result.fetchOne();
            if (!row.isNull(0)) {
                next_seq = row[0].get<int>() + 1;
            }
        }
        
        ss << std::setfill('0') << std::setw(3) << next_seq;
#endif
    } catch (const std::exception& err) {
        logger_->warn("Failed to generate sequential project code, using timestamp fallback: {}", err.what());
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count();
        ss << std::setfill('0') << std::setw(3) << (ms % 1000);
    }
    
    return ss.str();
}

} // namespace aeronautical