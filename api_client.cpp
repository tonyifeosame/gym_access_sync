#include "api_client.h"
#include "api_mock.h"
#include "database.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <chrono>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
using SocketType = SOCKET;
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
using SocketType = int;
#endif

static std::string extractJsonString(const std::string& text, const std::string& key)
{
    std::string needle = "\"" + key + "\"";
    auto pos = text.find(needle);
    if (pos == std::string::npos) {
        return {};
    }

    auto colon = text.find(':', pos + needle.size());
    if (colon == std::string::npos) {
        return {};
    }

    auto quote = text.find('"', colon);
    if (quote == std::string::npos) {
        return {};
    }

    auto endQuote = text.find('"', quote + 1);
    if (endQuote == std::string::npos) {
        return {};
    }

    return text.substr(quote + 1, endQuote - quote - 1);
}

static std::string extractJsonIntField(const std::string& text, const std::string& key, int fallback)
{
    std::string needle = "\"" + key + "\"";
    auto pos = text.find(needle);
    if (pos == std::string::npos) {
        return std::to_string(fallback);
    }

    auto colon = text.find(':', pos + needle.size());
    if (colon == std::string::npos) {
        return std::to_string(fallback);
    }

    auto start = colon + 1;
    while (start < text.size() && std::isspace(static_cast<unsigned char>(text[start]))) {
        start++;
    }

    auto end = start;
    while (end < text.size() && (std::isdigit(static_cast<unsigned char>(text[end])) || text[end] == '-')) {
        end++;
    }

    if (start == end) {
        return std::to_string(fallback);
    }

    return text.substr(start, end - start);
}

static bool extractJsonBoolField(const std::string& text, const std::string& key, bool fallback)
{
    std::string needle = "\"" + key + "\"";
    auto pos = text.find(needle);
    if (pos == std::string::npos) {
        return fallback;
    }

    auto colon = text.find(':', pos + needle.size());
    if (colon == std::string::npos) {
        return fallback;
    }

    auto start = colon + 1;
    while (start < text.size() && std::isspace(static_cast<unsigned char>(text[start]))) {
        start++;
    }

    if (text.compare(start, 4, "true") == 0) {
        return true;
    }
    if (text.compare(start, 5, "false") == 0) {
        return false;
    }
    return fallback;
}

static std::vector<Member> getMockCloudMembers()
{
    return {
        {"001", "John Doe", "template1", "ACTIVE", "2024-12-31", "2026-06-06 10:00:00"},
        {"002", "Jane Smith", "template2", "INACTIVE", "2023-06-30", "2026-06-06 10:05:00"},
        {"003", "Alice Johnson", "", "ACTIVE", "2025-03-15", "2026-06-06 10:10:00"},
        {"004", "", "template4", "ACTIVE", "2024-09-30", "2026-06-06 10:15:00"},
        {"005", "New Member", "template5", "ACTIVE", "2025-12-31", "2026-06-08 08:00:00"}
    };
}

static bool initializeSockets()
{
#ifdef _WIN32
    static bool initialized = false;
    if (!initialized) {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            return false;
        }
        initialized = true;
    }
#endif
    return true;
}

static void closeSocket(SocketType socket)
{
#ifdef _WIN32
    closesocket(socket);
#else
    close(socket);
#endif
}

struct HttpResponse {
    int statusCode;
    std::string body;
};

static std::optional<HttpResponse> httpGet(const std::string& url, const ApiConfig& config)
{
    if (!initializeSockets()) {
        return std::nullopt;
    }

    std::string address = url;
    if (address.rfind("http://", 0) == 0) {
        address = address.substr(7);
    } else {
        return std::nullopt;
    }

    std::string host;
    std::string path = "/";
    int port = 80;

    auto slashPos = address.find('/');
    if (slashPos != std::string::npos) {
        host = address.substr(0, slashPos);
        path = address.substr(slashPos);
    } else {
        host = address;
    }

    auto colonPos = host.find(':');
    if (colonPos != std::string::npos) {
        port = std::stoi(host.substr(colonPos + 1));
        host = host.substr(0, colonPos);
    }

    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    addrinfo* result = nullptr;
    if (getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &result) != 0) {
        return std::nullopt;
    }

    SocketType sock = INVALID_SOCKET;
    for (addrinfo* rp = result; rp != nullptr; rp = rp->ai_next) {
        sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
#ifdef _WIN32
        if (sock == INVALID_SOCKET) {
            continue;
        }
#else
        if (sock < 0) {
            continue;
        }
#endif
        if (connect(sock, rp->ai_addr, static_cast<int>(rp->ai_addrlen)) == 0) {
            break;
        }
        closeSocket(sock);
        sock = INVALID_SOCKET;
    }

    freeaddrinfo(result);

#ifdef _WIN32
    if (sock == INVALID_SOCKET) {
#else
    if (sock < 0) {
#endif
        return std::nullopt;
    }

    std::ostringstream request;
    request << "GET " << path << " HTTP/1.1\r\n";
    request << "Host: " << host << "\r\n";
    request << "Connection: close\r\n";
    request << "Accept: application/json\r\n";
    if (!config.api_key.empty()) {
        request << "X-API-Key: " << config.api_key << "\r\n";
    }
    request << "\r\n";

    std::string requestText = request.str();
    send(sock, requestText.c_str(), static_cast<int>(requestText.size()), 0);

    std::string response;
    char buffer[4096];
    int bytesReceived = 0;
    while ((bytesReceived = recv(sock, buffer, sizeof(buffer), 0)) > 0) {
        response.append(buffer, bytesReceived);
    }
    closeSocket(sock);

    auto headerEnd = response.find("\r\n\r\n");
    if (headerEnd == std::string::npos) {
        return std::nullopt;
    }

    std::string header = response.substr(0, headerEnd);
    std::string body = response.substr(headerEnd + 4);
    int statusCode = 0;

    auto firstLineEnd = header.find("\r\n");
    std::string statusLine = (firstLineEnd == std::string::npos) ? header : header.substr(0, firstLineEnd);
    std::istringstream statusStream(statusLine);
    std::string protocol;
    statusStream >> protocol >> statusCode;

    return HttpResponse{statusCode, body};
}

static std::string getCurrentTimestamp()
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
    oss << std::put_time(&local_tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

static std::optional<HttpResponse> httpPost(const std::string& url, const ApiConfig& config, const std::string& body, const std::string& contentType = "application/json")
{
    if (!initializeSockets()) {
        return std::nullopt;
    }

    std::string address = url;
    if (address.rfind("http://", 0) == 0) {
        address = address.substr(7);
    } else {
        return std::nullopt;
    }

    std::string host;
    std::string path = "/";
    int port = 80;

    auto slashPos = address.find('/');
    if (slashPos != std::string::npos) {
        host = address.substr(0, slashPos);
        path = address.substr(slashPos);
    } else {
        host = address;
    }

    auto colonPos = host.find(':');
    if (colonPos != std::string::npos) {
        port = std::stoi(host.substr(colonPos + 1));
        host = host.substr(0, colonPos);
    }

    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    addrinfo* result = nullptr;
    if (getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &result) != 0) {
        return std::nullopt;
    }

    SocketType sock = INVALID_SOCKET;
    for (addrinfo* rp = result; rp != nullptr; rp = rp->ai_next) {
        sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
#ifdef _WIN32
        if (sock == INVALID_SOCKET) {
            continue;
        }
#else
        if (sock < 0) {
            continue;
        }
#endif
        if (connect(sock, rp->ai_addr, static_cast<int>(rp->ai_addrlen)) == 0) {
            break;
        }
        closeSocket(sock);
        sock = INVALID_SOCKET;
    }

    freeaddrinfo(result);

#ifdef _WIN32
    if (sock == INVALID_SOCKET) {
#else
    if (sock < 0) {
#endif
        return std::nullopt;
    }

    std::ostringstream request;
    request << "POST " << path << " HTTP/1.1\r\n";
    request << "Host: " << host << "\r\n";
    request << "Connection: close\r\n";
    request << "Accept: application/json\r\n";
    request << "Content-Type: " << contentType << "\r\n";
    request << "Content-Length: " << body.size() << "\r\n";
    if (!config.api_key.empty()) {
        request << "X-API-Key: " << config.api_key << "\r\n";
    }
    request << "\r\n";
    request << body;

    std::string requestText = request.str();
    send(sock, requestText.c_str(), static_cast<int>(requestText.size()), 0);

    std::string response;
    char buffer[4096];
    int bytesReceived = 0;
    while ((bytesReceived = recv(sock, buffer, sizeof(buffer), 0)) > 0) {
        response.append(buffer, bytesReceived);
    }
    closeSocket(sock);

    auto headerEnd = response.find("\r\n\r\n");
    if (headerEnd == std::string::npos) {
        return std::nullopt;
    }

    std::string header = response.substr(0, headerEnd);
    std::string bodyResponse = response.substr(headerEnd + 4);
    int statusCode = 0;

    auto firstLineEnd = header.find("\r\n");
    std::string statusLine = (firstLineEnd == std::string::npos) ? header : header.substr(0, firstLineEnd);
    std::istringstream statusStream(statusLine);
    std::string protocol;
    statusStream >> protocol >> statusCode;

    return HttpResponse{statusCode, bodyResponse};
}

bool loadConfig(const std::string& configPath, ApiConfig& config)
{
    std::ifstream file(configPath);
    if (!file.is_open()) {
        return false;
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    std::string text = buffer.str();

    config.api_url = extractJsonString(text, "api_url");
    config.api_key = extractJsonString(text, "api_key");
    config.sync_interval_seconds = std::stoi(extractJsonIntField(text, "sync_interval_seconds", 60));
    config.site_name = extractJsonString(text, "site_name");
    config.offline_mode = extractJsonBoolField(text, "offline_mode", false);
    config.scanner.ip = extractJsonString(text, "scanner_ip");
    config.scanner.port = std::stoi(extractJsonIntField(text, "scanner_port", 4370));
    config.scanner.timeout = std::stoi(extractJsonIntField(text, "enrollment_timeout", 30));

    return !config.api_url.empty() && !config.api_key.empty();
}

ApiClient::ApiClient(const ApiConfig& config)
    : config_(config)
{
}

static std::optional<Member> parseMemberObject(const std::string& object)
{
    Member member;
    member.member_id = extractJsonString(object, "member_id");
    if (member.member_id.empty()) {
        return std::nullopt;
    }
    member.member_name = extractJsonString(object, "member_name");
    member.member_fingerprint_template = extractJsonString(object, "member_fingerprint_template");
    member.member_status = extractJsonString(object, "member_status");
    member.member_expiring_date = extractJsonString(object, "member_expiring_date");
    member.last_updated = extractJsonString(object, "last_updated");
    return member;
}

static std::vector<Member> parseMembersArray(const std::string& payload)
{
    std::vector<Member> members;
    size_t pos = 0;
    while (true) {
        pos = payload.find("{\"member_id\"", pos);
        if (pos == std::string::npos) {
            break;
        }
        size_t end = payload.find('}', pos);
        if (end == std::string::npos) {
            break;
        }
        std::string object = payload.substr(pos, end - pos + 1);
        if (auto memberOpt = parseMemberObject(object)) {
            members.push_back(*memberOpt);
        }
        pos = end + 1;
    }
    return members;
}

std::vector<Member> ApiClient::fetchAllMembers(sqlite3* localDb)
{
    std::string url = config_.api_url;
    if (!url.empty() && url.back() == '/') {
        url.pop_back();
    }
    url += "/members";

    auto response = httpGet(url, config_);
    if (response && response->statusCode == 200) {
        return parseMembersArray(response->body);
    }

    if (localDb && config_.offline_mode) {
        return getAllMembers(localDb);
    }

    return {};
}

std::vector<Member> ApiClient::fetchChangedMembers(const std::string& lastSyncTime, sqlite3* localDb)
{
    std::string url = config_.api_url;
    if (!url.empty() && url.back() == '/') {
        url.pop_back();
    }
    url += "/members/changes?since=" + lastSyncTime;

    auto response = httpGet(url, config_);
    if (response && response->statusCode == 200) {
        return parseMembersArray(response->body);
    }

    if (localDb && config_.offline_mode) {
        return getMembersChangedSince(localDb, lastSyncTime);
    }

    return {};
}

std::optional<Member> ApiClient::fetchMember(const std::string& member_id)
{
    std::string url = config_.api_url;
    if (!url.empty() && url.back() == '/') {
        url.pop_back();
    }
    url += "/members/" + member_id;

    auto response = httpGet(url, config_);
    if (response && response->statusCode == 200) {
        if (auto memberOpt = parseMemberObject(response->body)) {
            return memberOpt;
        }
    }

    return std::nullopt;
}

static std::vector<std::string> parsePendingEnrollmentIds(const std::string& payload)
{
    std::vector<std::string> memberIds;
    std::string needle = "\"member_id\":\"";
    size_t pos = 0;
    while ((pos = payload.find(needle, pos)) != std::string::npos) {
        pos += needle.size();
        size_t end = payload.find('"', pos);
        if (end == std::string::npos) {
            break;
        }
        memberIds.push_back(payload.substr(pos, end - pos));
        pos = end + 1;
    }
    return memberIds;
}

static std::optional<AccessResult> parseAccessResponse(const std::string& member_id, const HttpResponse& response)
{
    AccessResult accessResult;
    accessResult.member_id = member_id;
    accessResult.server_available = true;
    accessResult.source = "API";

    if (response.statusCode == 401) {
        accessResult.granted = false;
        accessResult.message = "Unauthorized API key";
        return accessResult;
    }

    if (response.statusCode == 404) {
        accessResult.granted = false;
        accessResult.message = "Member not found";
        accessResult.status = extractJsonString(response.body, "status");
        return accessResult;
    }

    if (response.statusCode >= 400) {
        accessResult.granted = false;
        accessResult.message = "Server error " + std::to_string(response.statusCode);
        return accessResult;
    }

    accessResult.granted = extractJsonBoolField(response.body, "granted", false);
    accessResult.status = extractJsonString(response.body, "status");
    accessResult.message = extractJsonString(response.body, "message");

    if (accessResult.message.empty()) {
        accessResult.message = accessResult.granted ? "Access Granted" : "Access Denied";
    }

    return accessResult;
}

static std::optional<AccessResult> fetchAccessFromLocal(sqlite3* localDb, const std::string& member_id)
{
    auto memberOpt = getMemberById(localDb, member_id);
    if (!memberOpt) {
        AccessResult accessResult;
        accessResult.member_id = member_id;
        accessResult.granted = false;
        accessResult.server_available = false;
        accessResult.source = "OFFLINE_CACHE";
        accessResult.message = "Offline cache: member not found";
        return accessResult;
    }

    AccessResult accessResult;
    accessResult.member_id = member_id;
    accessResult.status = memberOpt->member_status;
    accessResult.granted = memberOpt->member_status == "ACTIVE";
    accessResult.server_available = false;
    accessResult.source = "OFFLINE_CACHE";
    accessResult.message = accessResult.granted ? "Offline cache: Access Granted" : "Offline cache: Access Denied";
    return accessResult;
}

std::optional<AccessResult> ApiClient::fetchAccess(const std::string& member_id, sqlite3* localDb)
{
    std::string url = config_.api_url;
    if (!url.empty() && url.back() == '/') {
        url.pop_back();
    }
    url += "/access/" + member_id;

    auto response = httpGet(url, config_);
    if (!response) {
        if (localDb && config_.offline_mode) {
            return fetchAccessFromLocal(localDb, member_id);
        }

        AccessResult accessResult;
        accessResult.member_id = member_id;
        accessResult.granted = false;
        accessResult.server_available = false;
        accessResult.source = "API";
        accessResult.message = "Server unavailable";
        return accessResult;
    }

    auto parsed = parseAccessResponse(member_id, *response);
    if (!parsed) {
        if (localDb && config_.offline_mode) {
            return fetchAccessFromLocal(localDb, member_id);
        }

        AccessResult accessResult;
        accessResult.member_id = member_id;
        accessResult.granted = false;
        accessResult.server_available = false;
        accessResult.source = "API";
        accessResult.message = "Invalid server response";
        return accessResult;
    }

    return parsed;
}

bool ApiClient::startEnrollment(const std::string& member_id, sqlite3* localDb)
{
    std::string url = config_.api_url;
    if (!url.empty() && url.back() == '/') {
        url.pop_back();
    }
    url += "/enrollment/start";

    std::ostringstream payload;
    payload << "{\"member_id\":\"" << member_id << "\"}";

    auto response = httpPost(url, config_, payload.str());
    if (response && response->statusCode == 200) {
        return true;
    }

    if (localDb && config_.offline_mode) {
        if (auto memberOpt = getMemberById(localDb, member_id)) {
            Member member = *memberOpt;
            member.member_status = "PENDING_ENROLLMENT";
            member.last_updated = getCurrentTimestamp();
            return updateMember(localDb, member);
        }
    }

    return false;
}

std::vector<std::string> ApiClient::fetchPendingEnrollments(sqlite3* localDb)
{
    std::string url = config_.api_url;
    if (!url.empty() && url.back() == '/') {
        url.pop_back();
    }
    url += "/enrollment/pending";

    auto response = httpGet(url, config_);
    if (response && response->statusCode == 200) {
        return parsePendingEnrollmentIds(response->body);
    }

    if (localDb && config_.offline_mode) {
        auto pendingMembers = getMembersByStatus(localDb, "PENDING_ENROLLMENT");
        std::vector<std::string> ids;
        for (const auto& member : pendingMembers) {
            ids.push_back(member.member_id);
        }
        return ids;
    }

    return {};
}

bool ApiClient::submitEnrollmentResult(const std::string& member_id, const std::string& fingerprintTemplate, sqlite3* localDb)
{
    std::string url = config_.api_url;
    if (!url.empty() && url.back() == '/') {
        url.pop_back();
    }
    url += "/enrollment/result";

    std::ostringstream payload;
    payload << "{\"member_id\":\"" << member_id << "\",";
    payload << "\"fingerprint_template\":\"" << fingerprintTemplate << "\"}";

    auto response = httpPost(url, config_, payload.str());
    if (response && response->statusCode == 200) {
        return true;
    }

    if (localDb && config_.offline_mode) {
        if (auto memberOpt = getMemberById(localDb, member_id)) {
            Member member = *memberOpt;
            member.member_fingerprint_template = fingerprintTemplate;
            member.member_status = "ACTIVE";
            member.last_updated = getCurrentTimestamp();
            return updateMember(localDb, member);
        }
    }

    return false;
}
