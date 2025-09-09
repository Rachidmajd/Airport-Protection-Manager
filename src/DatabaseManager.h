#pragma once

#include <string>
#include <memory>
#include <spdlog/spdlog.h>
#include <thread>
#include <mutex>
#include <unordered_map>

// Conditional includes based on available MySQL API
#ifdef USE_MYSQL_C_API
    #include <mysql/mysql.h>
    #include <mysql/errmsg.h>
#else
    #include <mysqlx/xdevapi.h>
#endif

namespace aeronautical {

class DatabaseManager {
public:
    static DatabaseManager& getInstance();
    
    // Delete copy constructor and assignment operator
    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;
    
    void initialize(const std::string& host, int port, const std::string& user, 
                   const std::string& password, const std::string& database);
    
#ifdef USE_MYSQL_C_API
    MYSQL* getConnection();
    bool executeQuery(const std::string& query);
    MYSQL_RES* executeSelectQuery(const std::string& query);
#else
    mysqlx::Session& getSession();
    mysqlx::Schema getSchema();
#endif
    
    // Generate unique project code
    std::string generateProjectCode();
    
    // Common database operations
    bool isConnected();
    void reconnect();
    
    // Cleanup for application shutdown
    void cleanup();
    
private:
    DatabaseManager();
    ~DatabaseManager();

#ifdef USE_MYSQL_C_API
    // Thread-local connection management
    static thread_local MYSQL* thread_connection_;
    static std::mutex connections_mutex_;
    static std::unordered_map<std::thread::id, MYSQL*> active_connections_;
    
    // Connection parameters
    std::string host_;
    int port_;
    std::string user_;
    std::string password_;
    std::string database_name_;
    
    // Thread-safe connection methods
    void ensureThreadConnection();
    MYSQL* createNewConnection();
    void cleanupThreadConnection();
    void registerConnection(MYSQL* conn);
    void unregisterConnection(MYSQL* conn);
    
#else
    std::unique_ptr<mysqlx::Session> session_;
    std::string database_name_;
#endif
    
    std::shared_ptr<spdlog::logger> logger_;
    std::mutex logger_mutex_; // Protect logger access
    
    void ensureConnected();
    bool initialized_ = false;
};

} // namespace aeronautical