#pragma once

#include <crow.h>
#include "ConflictRepository.h"
#include "ProjectRepository.h"
#include "FlightProcedureRepository.h"
#include <memory>
#include <mutex> // Include for thread-safety

namespace aeronautical {

class ConflictController {
public:
    // Make the instance accessible via a static method
    static ConflictController& getInstance();
    
    // Delete copy and move constructors to prevent duplicates
    ConflictController(const ConflictController&) = delete;
    ConflictController& operator=(const ConflictController&) = delete;

    void analyzeProject(int project_id);

    void registerRoutes(crow::SimpleApp& app);
    crow::response getConflictsByProject(int project_id);

private:
    ConflictController(); // Make the constructor private
    
    std::unique_ptr<ConflictRepository> repository_;
    static std::unique_ptr<ConflictController> instance_;
    static std::once_flag once_flag_;
};

} // namespace aeronautical