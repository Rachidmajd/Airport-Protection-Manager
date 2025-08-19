#pragma once

#include <string>
#include <optional>
#include <chrono>
#include <json.hpp>

namespace aeronautical {

enum class ProjectStatus {
    Created,
    Pending,
    UnderReview,
    Accepted,
    Refused,
    Cancelled
};

enum class ProjectPriority {
    Low,
    Normal,
    High,
    Critical
};

// Convert enums to/from strings
std::string statusToString(ProjectStatus status);
ProjectStatus stringToStatus(const std::string& str);
std::string priorityToString(ProjectPriority priority);
ProjectPriority stringToPriority(const std::string& str);

struct Project {
    int id;
    std::string project_code;
    std::string title;
    std::optional<std::string> description;
    int demander_id;
    std::string demander_name;
    std::optional<std::string> demander_organization;
    std::string demander_email;
    std::optional<std::string> demander_phone;
    ProjectStatus status;
    ProjectPriority priority;
    std::optional<std::string> operation_type;
    std::optional<int> altitude_min;
    std::optional<int> altitude_max;
    std::optional<std::chrono::system_clock::time_point> start_date;
    std::optional<std::chrono::system_clock::time_point> end_date;
    std::optional<int> assigned_reviewer_id;
    std::optional<std::chrono::system_clock::time_point> review_deadline;
    std::optional<std::chrono::system_clock::time_point> approval_date;
    std::optional<std::string> rejection_reason;
    std::optional<std::string> comment;
    std::optional<std::string> internal_notes;
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point updated_at;
    
    // Computed fields from views
    int document_count = 0;
    int geometry_count = 0;
    int conflict_count = 0;
    
    // Convert to/from JSON
    nlohmann::json toJson() const;
    static Project fromJson(const nlohmann::json& j);
};

// Helper for datetime conversion
std::string timePointToString(const std::chrono::system_clock::time_point& tp);
std::chrono::system_clock::time_point stringToTimePoint(const std::string& str);

} // namespace aeronautical
