#include "api_mock.h"
#include <algorithm>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

static std::string escapeJson(const std::string& value)
{
    std::string escaped;
    for (char c : value) {
        if (c == '"') {
            escaped += "\\\"";
        } else if (c == '\\') {
            escaped += "\\\\";
        } else {
            escaped += c;
        }
    }
    return escaped;
}

static std::string getServerTime()
{
    auto now = std::chrono::system_clock::now();
    std::time_t tt = std::chrono::system_clock::to_time_t(now);
    std::tm local_tm;
#if defined(_WIN32) || defined(_WIN64)
    localtime_s(&local_tm, &tt);
#else
    localtime_r(&tt, &local_tm);
#endif
    std::ostringstream oss;
    oss << std::put_time(&local_tm, "%Y-%m-%dT%H:%M:%S");
    return oss.str();
}

static std::string toJson(const Member& member, bool full = true)
{
    std::ostringstream oss;
    oss << "{"
        << "\"member_id\":\"" << escapeJson(member.member_id) << "\",";
    if (full) {
        oss << "\"member_name\":\"" << escapeJson(member.member_name) << "\",";
        oss << "\"member_fingerprint_template\":\"" << escapeJson(member.member_fingerprint_template) << "\",";
        oss << "\"member_status\":\"" << escapeJson(member.member_status) << "\",";
        oss << "\"member_expiring_date\":\"" << escapeJson(member.member_expiring_date) << "\",";
        oss << "\"last_updated\":\"" << escapeJson(member.last_updated) << "\"";
    } else {
        oss << "\"member_name\":\"" << escapeJson(member.member_name) << "\",";
        oss << "\"member_status\":\"" << escapeJson(member.member_status) << "\"";
    }
    oss << "}";
    return oss.str();
}

static std::string getQueryValue(const std::string& query, const std::string& key)
{
    std::string prefix = key + "=";
    auto pos = query.find(prefix);
    if (pos == std::string::npos) {
        return {};
    }
    auto start = pos + prefix.size();
    auto end = query.find('&', start);
    return query.substr(start, end == std::string::npos ? std::string::npos : end - start);
}

static std::optional<Member> parseMemberObject(const std::string& object, bool full)
{
    auto findValue = [&](const std::string& field) -> std::string {
        std::string token = "\"" + field + "\":\"";
        auto start = object.find(token);
        if (start == std::string::npos) return {};
        start += token.size();
        auto end = object.find('"', start);
        if (end == std::string::npos) return {};
        return object.substr(start, end - start);
    };

    Member member;
    member.member_id = findValue("member_id");
    if (member.member_id.empty()) {
        return std::nullopt;
    }
    member.member_name = findValue("member_name");
    member.member_status = findValue("member_status");
    if (full) {
        member.member_fingerprint_template = findValue("member_fingerprint_template");
        member.member_expiring_date = findValue("member_expiring_date");
        member.last_updated = findValue("last_updated");
    }
    return member;
}

static std::vector<Member> parseMembersArray(const std::string& payload)
{
    std::vector<Member> members;
    std::string openToken = "{\"member_id\":\"";
    size_t pos = 0;
    while ((pos = payload.find(openToken, pos)) != std::string::npos) {
        auto objStart = payload.rfind('{', pos);
        if (objStart == std::string::npos) break;
        auto objEnd = payload.find('}', pos);
        if (objEnd == std::string::npos) break;
        std::string object = payload.substr(objStart, objEnd - objStart + 1);
        auto memberOpt = parseMemberObject(object, true);
        if (memberOpt) {
            members.push_back(*memberOpt);
        }
        pos = objEnd + 1;
    }
    return members;
}

static std::string parseJsonField(const std::string& payload, const std::string& field)
{
    std::string token = "\"" + field + "\":\"";
    auto start = payload.find(token);
    if (start == std::string::npos) {
        return {};
    }
    start += token.size();
    auto end = payload.find('"', start);
    if (end == std::string::npos) {
        return {};
    }
    return payload.substr(start, end - start);
}

static std::optional<Member> parseSingleMember(const std::string& payload)
{
    auto objectStart = payload.find('{');
    auto objectEnd = payload.rfind('}');
    if (objectStart == std::string::npos || objectEnd == std::string::npos || objectEnd <= objectStart) {
        return std::nullopt;
    }
    std::string object = payload.substr(objectStart, objectEnd - objectStart + 1);
    return parseMemberObject(object, false);
}

MockHttpResponse mockApiServer(const MockHttpRequest& request, const std::vector<Member>& cloudMembers)
{
    if (request.apiKey != "gym-secret-key") {
        return {401, "{\"success\":false,\"error\":\"Unauthorized\"}"};
    }

    if (request.method == "GET" && request.path == "/api/members") {
        std::ostringstream body;
        body << "{\"members\":[";
        for (size_t i = 0; i < cloudMembers.size(); ++i) {
            if (i) body << ",";
            body << toJson(cloudMembers[i]);
        }
        body << "]}";
        return {200, body.str()};
    }

    if (request.method == "GET" && request.path == "/api/members/changes") {
        std::string since = getQueryValue(request.query, "since");
        std::vector<Member> changed;
        for (const auto& member : cloudMembers) {
            if (!since.empty() && member.last_updated > since) {
                changed.push_back(member);
            }
        }

        std::ostringstream body;
        body << "{\"success\":true,\"server_time\":\"" << getServerTime() << "\",\"members\":[";
        for (size_t i = 0; i < changed.size(); ++i) {
            if (i) body << ",";
            body << toJson(changed[i]);
        }
        body << "]}";
        return {200, body.str()};
    }

    if (request.method == "GET" && request.path.rfind("/api/members/", 0) == 0) {
        std::string id = request.path.substr(std::string("/api/members/").size());
        for (const auto& member : cloudMembers) {
            if (member.member_id == id) {
                return {200, toJson(member, false)};
            }
        }
        return {404, "{\"error\":\"Not found\"}"};
    }

    if (request.method == "GET" && request.path.rfind("/api/access/", 0) == 0) {
        std::string id = request.path.substr(std::string("/api/access/").size());
        for (const auto& member : cloudMembers) {
            if (member.member_id == id) {
                bool access = member.member_status == "ACTIVE";
                std::ostringstream body;
                body << "{"
                     << "\"member_id\":\"" << escapeJson(member.member_id) << "\",";
                body << "\"status\":\"" << escapeJson(member.member_status) << "\",";
                body << "\"access\":" << (access ? "true" : "false");
                body << "}";
                return {200, body.str()};
            }
        }
        return {404, "{\"error\":\"Not found\"}"};
    }

    if (request.method == "POST" && request.path == "/api/enrollment/start") {
        std::string memberId = parseJsonField(request.body, "member_id");
        for (const auto& member : cloudMembers) {
            if (member.member_id == memberId) {
                return {200, "{\"success\":true,\"member_id\":\"" + escapeJson(memberId) + "\",\"status\":\"PENDING_ENROLLMENT\"}"};
            }
        }
        return {404, "{\"success\":false,\"error\":\"Member not found\"}"};
    }

    if (request.method == "GET" && request.path == "/api/enrollment/pending") {
        std::ostringstream body;
        body << "{\"success\":true,\"pending\":[";
        bool first = true;
        for (const auto& member : cloudMembers) {
            if (member.member_status == "PENDING_ENROLLMENT") {
                if (!first) {
                    body << ",";
                }
                body << "{\"member_id\":\"" << escapeJson(member.member_id) << "\",\"member_name\":\"" << escapeJson(member.member_name) << "\"}";
                first = false;
            }
        }
        body << "]}";
        return {200, body.str()};
    }

    if (request.method == "POST" && request.path == "/api/enrollment/result") {
        std::string memberId = parseJsonField(request.body, "member_id");
        std::string fingerprint = parseJsonField(request.body, "fingerprint_template");
        for (const auto& member : cloudMembers) {
            if (member.member_id == memberId) {
                std::ostringstream body;
                body << "{\"success\":true,\"member_id\":\"" << escapeJson(memberId) << "\",\"status\":\"ACTIVE\"}";
                return {200, body.str()};
            }
        }
        return {404, "{\"success\":false,\"error\":\"Member not found\"}"};
    }

    return {404, "{\"error\":\"Endpoint not found\"}"};
}

std::vector<Member> mockApiClientGetAllMembers(const std::vector<Member>& cloudMembers)
{
    MockHttpRequest request{"GET", "/api/members", "", "gym-secret-key"};
    MockHttpResponse response = mockApiServer(request, cloudMembers);
    if (response.status != 200) {
        return {};
    }
    return parseMembersArray(response.body);
}

std::vector<Member> mockApiClientGetChanges(const std::string& since, const std::vector<Member>& cloudMembers)
{
    MockHttpRequest request{"GET", "/api/members/changes", "since=" + since, "gym-secret-key"};
    MockHttpResponse response = mockApiServer(request, cloudMembers);
    if (response.status != 200) {
        return {};
    }
    return parseMembersArray(response.body);
}

std::optional<Member> mockApiClientGetMember(const std::string& member_id, const std::vector<Member>& cloudMembers)
{
    MockHttpRequest request{"GET", "/api/members/" + member_id, "", "gym-secret-key"};
    MockHttpResponse response = mockApiServer(request, cloudMembers);
    if (response.status != 200) {
        return std::nullopt;
    }
    return parseSingleMember(response.body);
}

std::optional<AccessInfo> mockApiClientGetAccess(const std::string& member_id, const std::vector<Member>& cloudMembers)
{
    MockHttpRequest request{"GET", "/api/access/" + member_id, "", "gym-secret-key"};
    MockHttpResponse response = mockApiServer(request, cloudMembers);
    if (response.status != 200) {
        return std::nullopt;
    }
    AccessInfo info;
    auto findValue = [&](const std::string& field) -> std::string {
        std::string token = "\"" + field + "\":\"";
        auto start = response.body.find(token);
        if (start == std::string::npos) return {};
        start += token.size();
        auto end = response.body.find('"', start);
        if (end == std::string::npos) return {};
        return response.body.substr(start, end - start);
    };

    info.member_id = findValue("member_id");
    info.status = findValue("status");
    info.access = response.body.find("\"access\":true") != std::string::npos;
    return info;
}
