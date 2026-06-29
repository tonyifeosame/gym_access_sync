#pragma once

#include <string>

// Canonical REST API paths (local server + terminal client).
// Optional /api/v1 prefix is accepted by the server for cloud compatibility.
namespace api {

inline constexpr const char* kV1Prefix = "/api/v1";

inline constexpr const char* kMembers = "/members";
inline constexpr const char* kMembersChanges = "/members/changes";
inline constexpr const char* kStats = "/stats";
inline constexpr const char* kEnrollmentStart = "/enrollment/start";
inline constexpr const char* kEnrollmentPending = "/enrollment/pending";
inline constexpr const char* kEnrollmentResult = "/enrollment/result";
inline constexpr const char* kAccessLogs = "/access/logs";
inline constexpr const char* kAccessLog = "/access/log";

inline std::string normalizePath(std::string path)
{
    if (path.rfind(kV1Prefix, 0) == 0) {
        path = path.substr(std::string(kV1Prefix).size());
        if (path.empty()) {
            path = "/";
        }
    }
    return path;
}

inline std::string memberPath(const std::string& memberId)
{
    return std::string(kMembers) + "/" + memberId;
}

inline std::string accessPath(const std::string& memberId)
{
    return std::string("/access/") + memberId;
}

inline std::string membersChangesQuery(const std::string& since)
{
    return std::string(kMembersChanges) + "?since=" + since;
}

} // namespace api
