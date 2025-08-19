#pragma once

#include <string>
#include <memory>
#include <spdlog/spdlog.h>
#include <mutex> // <--- Add this header
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
    
private:
    DatabaseManager();
    ~DatabaseManager();

    std::recursive_mutex db_mutex_; // <--- Add the mutex here
    
#ifdef USE_MYSQL_C_API
    MYSQL* connection_;
    std::string host_;
    int port_;
    std::string user_;
    std::string password_;
    std::string database_name_;
    void connectToDatabase();
    void cleanup();
#else
    std::unique_ptr<mysqlx::Session> session_;
    std::string database_name_;
#endif
    
    std::shared_ptr<spdlog::logger> logger_;
    
    void ensureConnected();
};

} // namespace aeronautical