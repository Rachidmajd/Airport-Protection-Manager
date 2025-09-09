#include "DatabaseManager.h"
#include <chrono>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace aeronautical {

#ifdef USE_MYSQL_C_API
// Static member definitions for thread-local storage
thread_local MYSQL* DatabaseManager::thread_connection_ = nullptr;
std::mutex DatabaseManager::connections_mutex_;
std::unordered_map<std::thread::id, MYSQL*> DatabaseManager::active_connections_;
#endif

DatabaseManager& DatabaseManager::getInstance() {
    static DatabaseManager instance;
    return instance;
}

DatabaseManager::DatabaseManager() {
#ifdef USE_MYSQL_C_API
    // Initialize MySQL library (thread-safe)
    if (mysql_library_init(0, nullptr, nullptr)) {
        throw std::runtime_error("Could not initialize MySQL library");
    }
#endif
}

DatabaseManager::~DatabaseManager() {
    cleanup();
}

void DatabaseManager::initialize(const std::string& host, int port, 
                                const std::string& user, const std::string& password, 
                                const std::string& database) {
    try {
        // Initialize logger with mutex protection
        std::lock_guard<std::mutex> logger_lock(logger_mutex_);
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
        
#ifdef USE_MYSQL_C_API
        // Store connection parameters for thread-local connections
        host_ = host;
        port_ = port;
        user_ = user;
        password_ = password;
        database_name_ = database;
        
        // Test initial connection to verify parameters
        MYSQL* test_conn = createNewConnection();
        if (!test_conn) {
            throw std::runtime_error("Failed to establish initial test connection");
        }
        
        // Close test connection - each thread will create its own
        mysql_close(test_conn);
        
        logger_->info("Database connection parameters validated for {}:{}/{} (MySQL C API)", host, port, database);
        
#else
        std::stringstream ss;
        ss << "mysqlx://" << user << ":" << password << "@" 
           << host << ":" << port << "/" << database;
        
        session_ = std::make_unique<mysqlx::Session>(ss.str());
        database_name_ = database;
        
        logger_->info("Database connection established to {}:{}/{} (MySQL Connector/C++)", host, port, database);
#endif

        initialized_ = true;
        
    } catch (const std::exception& err) {
        if (logger_) {
            logger_->error("Failed to initialize database: {}", err.what());
        }
        throw;
    }
}

#ifdef USE_MYSQL_C_API

MYSQL* DatabaseManager::createNewConnection() {
    MYSQL* conn = mysql_init(nullptr);
    if (!conn) {
        if (logger_) logger_->error("Failed to initialize MySQL connection");
        return nullptr;
    }
    
    // Set connection options
    unsigned int timeout = 10;
    mysql_options(conn, MYSQL_OPT_CONNECT_TIMEOUT, &timeout);
    
    // Enable auto-reconnect for this connection
    my_bool reconnect = 1;
    mysql_options(conn, MYSQL_OPT_RECONNECT, &reconnect);
    
    // Set charset
    mysql_options(conn, MYSQL_SET_CHARSET_NAME, "utf8mb4");
    
    // Connect to database
    if (!mysql_real_connect(conn, host_.c_str(), user_.c_str(), 
                           password_.c_str(), database_name_.c_str(), port_, nullptr, 0)) {
        std::string error = mysql_error(conn);
        unsigned int error_code = mysql_errno(conn);
        
        if (logger_) {
            logger_->error("Failed to connect to database: {} (Code: {})", error, error_code);
        }
        
        mysql_close(conn);
        return nullptr;
    }
    
    return conn;
}

void DatabaseManager::ensureThreadConnection() {
    if (!initialized_) {
        throw std::runtime_error("DatabaseManager not initialized");
    }
    
    // Check if we already have a valid connection for this thread
    if (thread_connection_) {
        // Test connection health
        if (mysql_ping(thread_connection_) == 0) {
            return; // Connection is healthy
        }
        
        // Connection is dead, clean it up
        if (logger_) {
            std::ostringstream oss;
            oss << std::this_thread::get_id();
            logger_->warn("Thread connection lost, reconnecting... Thread ID: {}", oss.str());
        }
        cleanupThreadConnection();
    }
    
    // Create new connection for this thread
    thread_connection_ = createNewConnection();
    if (!thread_connection_) {
        throw std::runtime_error("Failed to create thread-local MySQL connection");
    }
    
    // Register this connection for cleanup tracking
    registerConnection(thread_connection_);
    
    if (logger_) {
        std::ostringstream oss;
        oss << std::this_thread::get_id();
        logger_->debug("Created thread-local connection for thread: {}", oss.str());
    }
}

void DatabaseManager::registerConnection(MYSQL* conn) {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    active_connections_[std::this_thread::get_id()] = conn;
}

void DatabaseManager::unregisterConnection(MYSQL* conn) {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    auto thread_id = std::this_thread::get_id();
    auto it = active_connections_.find(thread_id);
    if (it != active_connections_.end() && it->second == conn) {
        active_connections_.erase(it);
    }
}

void DatabaseManager::cleanupThreadConnection() {
    if (thread_connection_) {
        unregisterConnection(thread_connection_);
        mysql_close(thread_connection_);
        thread_connection_ = nullptr;
    }
}

MYSQL* DatabaseManager::getConnection() {
    ensureThreadConnection();
    return thread_connection_;
}

bool DatabaseManager::executeQuery(const std::string& query) {
    try {
        ensureThreadConnection();
        
        if (logger_) {
            std::ostringstream oss;
            oss << std::this_thread::get_id();
            logger_->debug("Executing query on thread {}: {}", oss.str(), query);
        }
        
        if (mysql_query(thread_connection_, query.c_str())) {
            unsigned int error_code = mysql_errno(thread_connection_);
            const char* error_msg = mysql_error(thread_connection_);
            const char* sqlstate = mysql_sqlstate(thread_connection_);
            
            if (logger_) {
                std::ostringstream oss;
                oss << std::this_thread::get_id();
                logger_->error("Query failed on thread {}: {} - Error Code: {}, SQLSTATE: {}, Message: '{}'", 
                             oss.str(), query, error_code, 
                             sqlstate ? sqlstate : "unknown", 
                             error_msg ? error_msg : "no error message");
            }
            
            // If it's a connection error, force reconnection on next call
            if (error_code == CR_SERVER_GONE_ERROR || error_code == CR_SERVER_LOST) {
                cleanupThreadConnection();
            }
            
            return false;
        }
        
        if (logger_) {
            std::ostringstream oss;
            oss << std::this_thread::get_id();
            logger_->debug("Query executed successfully on thread {}", oss.str());
        }
        return true;
        
    } catch (const std::exception& e) {
        if (logger_) {
            std::ostringstream oss;
            oss << std::this_thread::get_id();
            logger_->error("Exception in executeQuery on thread {}: {}", oss.str(), e.what());
        }
        return false;
    }
}

MYSQL_RES* DatabaseManager::executeSelectQuery(const std::string& query) {
    try {
        ensureThreadConnection();
        
        if (logger_) {
            std::ostringstream oss;
            oss << std::this_thread::get_id();
            logger_->info("=== EXECUTING SELECT QUERY ON THREAD {} ===", oss.str());
            logger_->debug("Query: {}", query);
            logger_->debug("Query length: {} characters", query.length());
        }
        
        if (mysql_query(thread_connection_, query.c_str())) {
            unsigned int error_code = mysql_errno(thread_connection_);
            const char* error_msg = mysql_error(thread_connection_);
            const char* sqlstate = mysql_sqlstate(thread_connection_);
            
            if (logger_) {
                std::ostringstream oss;
                oss << std::this_thread::get_id();
                logger_->error("SELECT query failed on thread {}: {} - Error Code: {}, SQLSTATE: {}, Message: '{}'", 
                             oss.str(), query, error_code, 
                             sqlstate ? sqlstate : "unknown", 
                             error_msg ? error_msg : "no error message");
            }
            
            // If it's a connection error, force reconnection on next call
            if (error_code == CR_SERVER_GONE_ERROR || error_code == CR_SERVER_LOST) {
                cleanupThreadConnection();
            }
            
            return nullptr;
        }
        
        if (logger_) {
            std::ostringstream oss;
            oss << std::this_thread::get_id();
            logger_->debug("mysql_query executed successfully on thread {}", oss.str());
        }
        
        MYSQL_RES* result = mysql_store_result(thread_connection_);
        if (!result) {
            // Check if this was supposed to return a result set
            if (mysql_field_count(thread_connection_) > 0) {
                if (logger_) {
                    std::ostringstream oss;
                    oss << std::this_thread::get_id();
                    logger_->error("Failed to store result for query on thread {}: {} - Error: '{}'", 
                                 oss.str(), query, mysql_error(thread_connection_));
                }
                return nullptr;
            } else {
                // Query didn't return a result set (e.g., INSERT, UPDATE, DELETE)
                if (logger_) {
                    std::ostringstream oss;
                    oss << std::this_thread::get_id();
                    logger_->debug("Query completed without result set on thread {} (non-SELECT query)", 
                                 oss.str());
                }
                return nullptr;
            }
        } else {
            unsigned long num_rows = mysql_num_rows(result);
            unsigned int num_fields = mysql_num_fields(result);
            
            if (logger_) {
                std::ostringstream oss;
                oss << std::this_thread::get_id();
                logger_->info("Query returned {} rows, {} fields on thread {}", 
                            num_rows, num_fields, oss.str());
                
                if (num_rows == 0) {
                    logger_->warn("WARNING: Query returned 0 rows on thread {}!", oss.str());
                }
                
                // Log field names for debugging
                MYSQL_FIELD* fields = mysql_fetch_fields(result);
                if (fields) {
                    logger_->debug("Field names on thread {}: ", oss.str());
                    for (unsigned int i = 0; i < num_fields; i++) {
                        logger_->debug("  {}: {}", i, fields[i].name ? fields[i].name : "NULL");
                    }
                }
            }
        }
        
        if (logger_) {
            std::ostringstream oss;
            oss << std::this_thread::get_id();
            logger_->info("=== SELECT QUERY COMPLETED SUCCESSFULLY ON THREAD {} ===", oss.str());
        }
        return result;
        
    } catch (const std::exception& e) {
        if (logger_) {
            std::ostringstream oss;
            oss << std::this_thread::get_id();
            logger_->error("Exception in executeSelectQuery on thread {}: {}", oss.str(), e.what());
        }
        return nullptr;
    }
}

bool DatabaseManager::isConnected() {
    try {
        if (!thread_connection_) {
            return false;
        }
        
        // Use ping to test connection
        int ping_result = mysql_ping(thread_connection_);
        if (ping_result != 0) {
            if (logger_) {
                unsigned int error_code = mysql_errno(thread_connection_);
                std::ostringstream oss;
                oss << std::this_thread::get_id();
                logger_->debug("Connection ping failed on thread {} with error code: {}, message: '{}'", 
                             oss.str(), error_code, mysql_error(thread_connection_));
            }
            return false;
        }
        
        return true;
    } catch (const std::exception& e) {
        if (logger_) {
            std::ostringstream oss;
            oss << std::this_thread::get_id();
            logger_->error("Exception in isConnected on thread {}: {}", oss.str(), e.what());
        }
        return false;
    }
}

void DatabaseManager::reconnect() {
    if (logger_) {
        std::ostringstream oss;
        oss << std::this_thread::get_id();
        logger_->info("Reconnecting database on thread {}...", oss.str());
    }
    
    cleanupThreadConnection();
    ensureThreadConnection();
}

void DatabaseManager::cleanup() {
    try {
        if (logger_) {
            logger_->info("Cleaning up DatabaseManager...");
        }
        
        // Clean up thread-local connection
        cleanupThreadConnection();
        
        // Clean up all tracked connections
        {
            std::lock_guard<std::mutex> lock(connections_mutex_);
            for (auto& [thread_id, conn] : active_connections_) {
                if (conn) {
                    mysql_close(conn);
                }
            }
            active_connections_.clear();
        }
        
        // Cleanup MySQL library
        mysql_library_end();
        
        if (logger_) {
            logger_->info("DatabaseManager cleanup completed");
        }
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->error("Error during DatabaseManager cleanup: {}", e.what());
        }
    }
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

bool DatabaseManager::isConnected() {
    if (!session_) return false;
    try {
        session_->sql("SELECT 1").execute();
        return true;
    } catch (const mysqlx::Error&) {
        return false;
    }
}

void DatabaseManager::reconnect() {
    throw std::runtime_error("Reconnection not implemented for Connector/C++ version");
}

void DatabaseManager::cleanup() {
    if (session_) {
        session_->close();
        session_.reset();
    }
}

#endif

void DatabaseManager::ensureConnected() {
    if (!initialized_) {
        throw std::runtime_error("Database not initialized");
    }

#ifdef USE_MYSQL_C_API
    ensureThreadConnection();
#else
    if (!session_) {
        throw std::runtime_error("Database session not available");
    }
    
    try {
        session_->sql("SELECT 1").execute();
    } catch (const mysqlx::Error& err) {
        if (logger_) {
            logger_->warn("Database connection lost, attempting to reconnect...");
        }
        throw std::runtime_error("Database connection lost");
    }
#endif
}

std::string DatabaseManager::generateProjectCode() {
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
        if (logger_) {
            logger_->warn("Failed to generate sequential project code, using timestamp fallback: {}", err.what());
        }
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count();
        ss << std::setfill('0') << std::setw(3) << (ms % 1000);
    }
    
    return ss.str();
}

} // namespace aeronautic