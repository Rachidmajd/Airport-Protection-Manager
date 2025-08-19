#pragma once

#include "FlightProcedure.h"
#include <vector>
#include <optional>
#include <memory>
#include <spdlog/spdlog.h>

// Conditional includes based on available MySQL API
#ifdef USE_MYSQL_C_API
    #include <mysql/mysql.h>
#else
    #include <mysqlx/xdevapi.h>
#endif

namespace aeronautical {

struct FlightProcedureFilter {
    std::optional<ProcedureType> type;
    std::optional<std::string> airport_icao;
    std::optional<std::string> runway;
    std::optional<bool> is_active;
    bool include_segments = true;
    bool include_protections = true;
    int limit = 100;
    int offset = 0;
};

class FlightProcedureRepository {
public:
    FlightProcedureRepository();
    ~FlightProcedureRepository() = default;
    
    // CRUD operations
    std::vector<FlightProcedure> findAll(const FlightProcedureFilter& filter = {});
    std::optional<FlightProcedure> findById(int id);
    std::optional<FlightProcedure> findByCode(const std::string& code);
    std::vector<FlightProcedure> findByAirport(const std::string& airport_icao);
    FlightProcedure create(const FlightProcedure& procedure);
    bool update(int id, const FlightProcedure& procedure);
    bool deleteById(int id);
    
    // Segment operations
    std::vector<ProcedureSegment> getSegments(int procedure_id);
    ProcedureSegment createSegment(const ProcedureSegment& segment);
    bool updateSegment(int id, const ProcedureSegment& segment);
    bool deleteSegment(int id);
    
    // Protection operations
    std::vector<ProcedureProtection> getProtections(int procedure_id);
    ProcedureProtection createProtection(const ProcedureProtection& protection);
    bool updateProtection(int id, const ProcedureProtection& protection);
    bool deleteProtection(int id);
    
    // Statistics
    int count(const FlightProcedureFilter& filter = {});

    std::vector<ProcedureProtection> findAllActiveProtections();
    
private:
    std::shared_ptr<spdlog::logger> logger_;
    
#ifdef USE_MYSQL_C_API
    FlightProcedure rowToProcedure(MYSQL_ROW row, unsigned long* lengths);
    ProcedureSegment rowToSegment(MYSQL_ROW row, unsigned long* lengths);
    ProcedureProtection rowToProtection(MYSQL_ROW row, unsigned long* lengths);
    std::string buildSelectQuery() const;
    std::string buildSegmentSelectQuery() const;
    std::string buildProtectionSelectQuery() const;
    std::string buildCountQuery() const;
#else
    FlightProcedure rowToProcedure(mysqlx::Row& row);
    ProcedureSegment rowToSegment(mysqlx::Row& row);
    ProcedureProtection rowToProtection(mysqlx::Row& row);
#endif
    
    void loadRelatedData(FlightProcedure& procedure, bool include_segments, bool include_protections);
};

} // namespace aeronautical