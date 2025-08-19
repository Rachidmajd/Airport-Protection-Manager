#include "Project.h"
#include <sstream>
#include <iomanip>
#include <ctime>

namespace aeronautical {

std::string statusToString(ProjectStatus status) {
    switch (status) {
        case ProjectStatus::Created: return "Created";
        case ProjectStatus::Pending: return "Pending";
        case ProjectStatus::UnderReview: return "Under_Review";
        case ProjectStatus::Accepted: return "Accepted";
        case ProjectStatus::Refused: return "Refused";
        case ProjectStatus::Cancelled: return "Cancelled";
        default: return "Created";
    }
}

ProjectStatus stringToStatus(const std::string& str) {
    if (str == "Pending") return ProjectStatus::Pending;
    if (str == "Under_Review") return ProjectStatus::UnderReview;
    if (str == "Accepted") return ProjectStatus::Accepted;
    if (str == "Refused") return ProjectStatus::Refused;
    if (str == "Cancelled") return ProjectStatus::Cancelled;
    return ProjectStatus::Created;
}

std::string priorityToString(ProjectPriority priority) {
    switch (priority) {
        case ProjectPriority::Low: return "Low";
        case ProjectPriority::Normal: return "Normal";
        case ProjectPriority::High: return "High";
        case ProjectPriority::Critical: return "Critical";
        default: return "Normal";
    }
}

ProjectPriority stringToPriority(const std::string& str) {
    if (str == "Low") return ProjectPriority::Low;
    if (str == "High") return ProjectPriority::High;
    if (str == "Critical") return ProjectPriority::Critical;
    return ProjectPriority::Normal;
}

std::string timePointToString(const std::chrono::system_clock::time_point& tp) {
    auto time_t = std::chrono::system_clock::to_time_t(tp);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

std::chrono::system_clock::time_point stringToTimePoint(const std::string& str) {
    std::tm tm = {};
    std::istringstream ss(str);
    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    return std::chrono::system_clock::from_time_t(std::mktime(&tm));
}

nlohmann::json Project::toJson() const {
    nlohmann::json j;
    j["id"] = id;
    j["project_code"] = project_code;
    j["title"] = title;
    if (description) j["description"] = *description;
    j["demander_id"] = demander_id;
    j["demander_name"] = demander_name;
    if (demander_organization) j["demander_organization"] = *demander_organization;
    j["demander_email"] = demander_email;
    if (demander_phone) j["demander_phone"] = *demander_phone;
    j["status"] = statusToString(status);
    j["priority"] = priorityToString(priority);
    if (operation_type) j["operation_type"] = *operation_type;
    if (altitude_min) j["altitude_min"] = *altitude_min;
    if (altitude_max) j["altitude_max"] = *altitude_max;
    if (start_date) j["start_date"] = timePointToString(*start_date);
    if (end_date) j["end_date"] = timePointToString(*end_date);
    if (assigned_reviewer_id) j["assigned_reviewer_id"] = *assigned_reviewer_id;
    if (review_deadline) j["review_deadline"] = timePointToString(*review_deadline);
    if (approval_date) j["approval_date"] = timePointToString(*approval_date);
    if (rejection_reason) j["rejection_reason"] = *rejection_reason;
    if (comment) j["comment"] = *comment;
    if (internal_notes) j["internal_notes"] = *internal_notes;
    j["created_at"] = timePointToString(created_at);
    j["updated_at"] = timePointToString(updated_at);
    j["document_count"] = document_count;
    j["geometry_count"] = geometry_count;
    j["conflict_count"] = conflict_count;
    
    return j;
}

Project Project::fromJson(const nlohmann::json& j) {
    Project p;
    
    if (j.contains("id")) p.id = j["id"];
    if (j.contains("project_code")) p.project_code = j["project_code"];
    if (j.contains("title")) p.title = j["title"];
    if (j.contains("description") && !j["description"].is_null()) 
        p.description = j["description"];
    if (j.contains("demander_id")) p.demander_id = j["demander_id"];
    if (j.contains("demander_name")) p.demander_name = j["demander_name"];
    if (j.contains("demander_organization") && !j["demander_organization"].is_null()) 
        p.demander_organization = j["demander_organization"];
    if (j.contains("demander_email")) p.demander_email = j["demander_email"];
    if (j.contains("demander_phone") && !j["demander_phone"].is_null()) 
        p.demander_phone = j["demander_phone"];
    if (j.contains("status")) p.status = stringToStatus(j["status"]);
    if (j.contains("priority")) p.priority = stringToPriority(j["priority"]);
    if (j.contains("operation_type") && !j["operation_type"].is_null()) 
        p.operation_type = j["operation_type"];
    if (j.contains("altitude_min") && !j["altitude_min"].is_null()) 
        p.altitude_min = j["altitude_min"];
    if (j.contains("altitude_max") && !j["altitude_max"].is_null()) 
        p.altitude_max = j["altitude_max"];
    if (j.contains("start_date") && !j["start_date"].is_null()) 
        p.start_date = stringToTimePoint(j["start_date"]);
    if (j.contains("end_date") && !j["end_date"].is_null()) 
        p.end_date = stringToTimePoint(j["end_date"]);
    if (j.contains("assigned_reviewer_id") && !j["assigned_reviewer_id"].is_null()) 
        p.assigned_reviewer_id = j["assigned_reviewer_id"];
    if (j.contains("review_deadline") && !j["review_deadline"].is_null()) 
        p.review_deadline = stringToTimePoint(j["review_deadline"]);
    if (j.contains("approval_date") && !j["approval_date"].is_null()) 
        p.approval_date = stringToTimePoint(j["approval_date"]);
    if (j.contains("rejection_reason") && !j["rejection_reason"].is_null()) 
        p.rejection_reason = j["rejection_reason"];
    if (j.contains("comment") && !j["comment"].is_null()) 
        p.comment = j["comment"];
    if (j.contains("internal_notes") && !j["internal_notes"].is_null()) 
        p.internal_notes = j["internal_notes"];
    
    return p;
}

} // namespace aeronautical
