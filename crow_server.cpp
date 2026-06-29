// REST server implementation (RestServer class). Linked by main.cpp and gym_server_main.cpp.
#include "rest_server.h"
#include "api_paths.h"
#include "database.h"
#include "member.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cctype>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
using SocketType = SOCKET;
using socklen_t = int;
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
using SocketType = int;
#endif

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

static std::string jsonEscape(const std::string& value)
{
    std::string escaped;
    escaped.reserve(value.size());
    for (char c : value) {
        switch (c) {
            case '"': escaped += "\\\""; break;
            case '\\': escaped += "\\\\"; break;
            case '\b': escaped += "\\b"; break;
            case '\f': escaped += "\\f"; break;
            case '\n': escaped += "\\n"; break;
            case '\r': escaped += "\\r"; break;
            case '\t': escaped += "\\t"; break;
            default: escaped += c; break;
        }
    }
    return escaped;
}

static std::string memberToJson(const Member& member)
{
    std::ostringstream oss;
    oss << "{"
        << "\"member_id\":\"" << jsonEscape(member.member_id) << "\",";
    oss << "\"member_name\":\"" << jsonEscape(member.member_name) << "\",";
    oss << "\"member_fingerprint_template\":\"" << jsonEscape(member.member_fingerprint_template) << "\",";
    oss << "\"member_status\":\"" << jsonEscape(member.member_status) << "\",";
    oss << "\"member_expiring_date\":\"" << jsonEscape(member.member_expiring_date) << "\",";
    oss << "\"last_updated\":\"" << jsonEscape(member.last_updated) << "\"";
    oss << "}";
    return oss.str();
}

static std::string jsonError(const std::string& message)
{
    std::ostringstream oss;
    oss << "{\"success\":false,\"error\":\"" << jsonEscape(message) << "\"}";
    return oss.str();
}

static std::string parseJsonField(const std::string& json, const std::string& key)
{
    std::string token = "\"" + key + "\"";
    auto pos = json.find(token);
    if (pos == std::string::npos) {
        return {};
    }
    auto colon = json.find(':', pos + token.size());
    if (colon == std::string::npos) {
        return {};
    }
    auto quote = json.find('"', colon);
    if (quote == std::string::npos) {
        return {};
    }
    auto endQuote = json.find('"', quote + 1);
    if (endQuote == std::string::npos) {
        return {};
    }
    return json.substr(quote + 1, endQuote - quote - 1);
}


static bool parseMemberFromBody(const std::string& body, Member& member)
{
    std::string name = parseJsonField(body, "member_name");
    std::string fingerprint = parseJsonField(body, "member_fingerprint_template");
    std::string status = parseJsonField(body, "member_status");
    std::string expiring = parseJsonField(body, "member_expiring_date");
    std::string lastUpdated = parseJsonField(body, "last_updated");

    if (name.empty() && fingerprint.empty() && status.empty() && expiring.empty() && lastUpdated.empty()) {
        return false;
    }

    member.member_name = name;
    member.member_fingerprint_template = fingerprint;
    member.member_status = status;
    member.member_expiring_date = expiring;
    member.last_updated = lastUpdated.empty() ? getCurrentTimestamp() : lastUpdated;
    return true;
}

static std::string toLower(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return std::tolower(c); });
    return text;
}

static std::string urlDecode(const std::string& value)
{
    std::ostringstream out;
    for (size_t i = 0; i < value.size(); ++i) {
        char c = value[i];
        if (c == '+') {
            out << ' ';
        } else if (c == '%' && i + 2 < value.size()) {
            std::string hex = value.substr(i + 1, 2);
            char decoded = static_cast<char>(std::strtol(hex.c_str(), nullptr, 16));
            out << decoded;
            i += 2;
        } else {
            out << c;
        }
    }
    return out.str();
}

struct HttpRequest {
    std::string method;
    std::string path;
    std::string query;
    std::map<std::string, std::string> headers;
    std::string body;
};

static std::string trim(const std::string& value)
{
    size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start]))) {
        start++;
    }
    size_t end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
        end--;
    }
    return value.substr(start, end - start);
}

static HttpRequest parseHttpRequest(const std::string& requestText)
{
    HttpRequest request;
    std::istringstream stream(requestText);
    std::string line;
    if (!std::getline(stream, line)) {
        return request;
    }

    std::istringstream requestLine(line);
    requestLine >> request.method;
    std::string target;
    requestLine >> target;
    auto queryPos = target.find('?');
    if (queryPos != std::string::npos) {
        request.path = target.substr(0, queryPos);
        request.query = target.substr(queryPos + 1);
    } else {
        request.path = target;
    }

    while (std::getline(stream, line) && line != "\r" && !line.empty()) {
        auto delimiter = line.find(':');
        if (delimiter != std::string::npos) {
            std::string headerName = trim(line.substr(0, delimiter));
            std::string headerValue = trim(line.substr(delimiter + 1));
            if (!headerValue.empty() && headerValue.back() == '\r') {
                headerValue.pop_back();
            }
            request.headers[toLower(headerName)] = headerValue;
        }
    }

    if (request.headers.count("content-length") > 0) {
        size_t length = std::stoul(request.headers["content-length"]);
        request.body.resize(length);
        stream.read(&request.body[0], length);
    }

    return request;
}

static void sendResponse(SocketType client, int statusCode, const std::string& body, const std::string& contentType = "application/json")
{
    std::ostringstream oss;
    oss << "HTTP/1.1 " << statusCode << " ";
    switch (statusCode) {
        case 200: oss << "OK"; break;
        case 201: oss << "Created"; break;
        case 204: oss << "No Content"; break;
        case 400: oss << "Bad Request"; break;
        case 404: oss << "Not Found"; break;
        case 409: oss << "Conflict"; break;
        case 500: oss << "Internal Server Error"; break;
        default: oss << "Error"; break;
    }
    oss << "\r\n";
    oss << "Content-Type: " << contentType << "\r\n";
    oss << "Content-Length: " << body.size() << "\r\n";
    oss << "Access-Control-Allow-Origin: *\r\n";
    oss << "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\r\n";
    oss << "Access-Control-Allow-Headers: Content-Type, X-API-Key\r\n";
    oss << "Connection: close\r\n";
    oss << "\r\n";
    oss << body;

    std::string response = oss.str();
    send(client, response.c_str(), static_cast<int>(response.size()), 0);
}

static std::string handleMemberCollection(sqlite3* db, const HttpRequest& request)
{
    if (request.method == "GET") {
        auto members = getAllMembers(db);
        std::ostringstream oss;
        oss << "{\"success\":true,\"members\":[";
        for (size_t i = 0; i < members.size(); ++i) {
            if (i > 0) {
                oss << ",";
            }
            oss << memberToJson(members[i]);
        }
        oss << "]}";
        return oss.str();
    }

    if (request.method == "POST") {
        Member member;
        parseMemberFromBody(request.body, member);
        member.member_id = parseJsonField(request.body, "member_id");
        if (member.member_id.empty() || member.member_name.empty()) {
            return jsonError("member_id and member_name are required");
        }
        if (member.member_status.empty()) {
            member.member_status = "PENDING_ENROLLMENT";
        }

        if (memberExists(db, member.member_id)) {
            return jsonError("Member already exists");
        }

        if (!insertMember(db, member)) {
            return jsonError("Failed to insert member");
        }

        std::ostringstream oss;
        oss << "{\"success\":true,\"member\":" << memberToJson(member) << "}";
        return oss.str();
    }

    return jsonError("Unsupported method for /members");
}

static std::string handleMemberItem(sqlite3* db, const HttpRequest& request, const std::string& memberId)
{
    if (request.method == "GET") {
        auto member = getMemberById(db, memberId);
        if (!member) {
            return jsonError("Member not found");
        }
        std::ostringstream oss;
        oss << "{\"success\":true,\"member\":" << memberToJson(*member) << "}";
        return oss.str();
    }

    if (request.method == "PUT") {
        auto existing = getMemberById(db, memberId);
        if (!existing) {
            return jsonError("Member not found");
        }

        Member updated = *existing;
        updated.member_id = memberId;
        if (!parseMemberFromBody(request.body, updated)) {
            return jsonError("Invalid member payload");
        }

        if (!updateMember(db, updated)) {
            return jsonError("Failed to update member");
        }

        std::ostringstream oss;
        oss << "{\"success\":true,\"member\":" << memberToJson(updated) << "}";
        return oss.str();
    }

    if (request.method == "DELETE") {
        if (!memberExists(db, memberId)) {
            return jsonError("Member not found");
        }

        if (!deleteMember(db, memberId)) {
            return jsonError("Failed to delete member");
        }

        return "{\"success\":true}";
    }

    return jsonError("Unsupported method for /members/{id}");
}

static std::string handleEnrollmentStart(sqlite3* db, const std::string& body)
{
    std::string memberId = parseJsonField(body, "member_id");
    if (memberId.empty()) {
        return jsonError("Invalid enrollment request");
    }

    auto memberOpt = getMemberById(db, memberId);
    if (!memberOpt) {
        return jsonError("Member not found");
    }

    Member member = *memberOpt;
    member.member_status = "PENDING_ENROLLMENT";
    member.last_updated = getCurrentTimestamp();
    if (!updateMember(db, member)) {
        return jsonError("Failed to update member state");
    }

    std::ostringstream oss;
    oss << "{\"success\":true,\"member_id\":\"" << jsonEscape(memberId) << "\",\"status\":\"PENDING_ENROLLMENT\"}";
    return oss.str();
}

static std::string handleEnrollmentPending(sqlite3* db)
{
    auto pendingMembers = getMembersByStatus(db, "PENDING_ENROLLMENT");
    std::ostringstream oss;
    oss << "{\"success\":true,\"pending\":[";
    for (size_t i = 0; i < pendingMembers.size(); ++i) {
        if (i > 0) {
            oss << ",";
        }
        oss << "{\"member_id\":\"" << jsonEscape(pendingMembers[i].member_id) << "\",\"member_name\":\"" << jsonEscape(pendingMembers[i].member_name) << "\"}";
    }
    oss << "]}";
    return oss.str();
}

static std::string handleEnrollmentResult(sqlite3* db, const std::string& body)
{
    std::string memberId = parseJsonField(body, "member_id");
    std::string fingerprint = parseJsonField(body, "member_fingerprint_template");
    if (fingerprint.empty()) {
        fingerprint = parseJsonField(body, "fingerprint_template");
    }
    if (memberId.empty() || fingerprint.empty()) {
        return jsonError("Invalid enrollment result payload");
    }

    auto memberOpt = getMemberById(db, memberId);
    if (!memberOpt) {
        return jsonError("Member not found");
    }

    Member member = *memberOpt;
    member.member_fingerprint_template = fingerprint;
    member.member_status = "ACTIVE";
    member.last_updated = getCurrentTimestamp();
    if (!updateMember(db, member)) {
        return jsonError("Failed to update member data");
    }

    std::ostringstream oss;
    oss << "{\"success\":true,\"member_id\":\"" << jsonEscape(memberId) << "\",\"status\":\"ACTIVE\"}";
    return oss.str();
}

static std::string handleAccessCheck(sqlite3* db, const std::string& memberId)
{
    auto member = getMemberById(db, memberId);
    if (!member) {
        return jsonError("Member not found");
    }

    bool granted = member->member_status == "ACTIVE";
    std::ostringstream oss;
    oss << "{"
        << "\"success\":true,"
        << "\"member_id\":\"" << jsonEscape(memberId) << "\",";
    oss << "\"status\":\"" << jsonEscape(member->member_status) << "\",";
    oss << "\"granted\":" << (granted ? "true" : "false") << ",";
    oss << "\"message\":\"" << jsonEscape(granted ? "Access Granted" : "Access Denied") << "\"";
    oss << "}";
    return oss.str();
}

static std::string accessLogToJson(const AccessLog& log)
{
    std::ostringstream oss;
    oss << "{"
        << "\"id\":" << log.id << ","
        << "\"member_id\":\"" << jsonEscape(log.member_id) << "\",";
    oss << "\"granted\":" << (log.granted ? "true" : "false") << ",";
    oss << "\"reason\":\"" << jsonEscape(log.reason) << "\",";
    oss << "\"timestamp\":\"" << jsonEscape(log.timestamp) << "\",";
    oss << "\"source\":\"" << jsonEscape(log.source) << "\"";
    oss << "}";
    return oss.str();
}

static std::string accessLogsToJson(const std::vector<AccessLog>& logs)
{
    std::ostringstream oss;
    oss << "{\"success\":true,\"logs\":[";
    for (size_t i = 0; i < logs.size(); ++i) {
        if (i > 0) {
            oss << ",";
        }
        oss << accessLogToJson(logs[i]);
    }
    oss << "]}";
    return oss.str();
}

static std::string getQueryParam(const std::string& query, const std::string& key)
{
    std::istringstream stream(query);
    std::string pair;
    while (std::getline(stream, pair, '&')) {
        auto eq = pair.find('=');
        if (eq == std::string::npos) {
            continue;
        }
        if (pair.substr(0, eq) == key) {
            return urlDecode(pair.substr(eq + 1));
        }
    }
    return {};
}

static std::string handleAccessLogs(sqlite3* db, const HttpRequest& request)
{
    std::string date = getQueryParam(request.query, "date");
    auto logs = date.empty() ? getAccessLogs(db) : getLogsByDate(db, date);
    return accessLogsToJson(logs);
}

static std::string handleAccessLogsByMember(sqlite3* db, const std::string& memberId)
{
    auto logs = getLogsByMember(db, memberId);
    return accessLogsToJson(logs);
}

static std::string handleAccessLogPost(sqlite3* db, const std::string& body)
{
    std::string memberId = parseJsonField(body, "member_id");
    if (memberId.empty()) {
        return jsonError("member_id is required");
    }
    std::string grantedStr = toLower(parseJsonField(body, "granted"));
    bool granted = grantedStr == "true" || grantedStr == "1";
    std::string reason = parseJsonField(body, "reason");
    std::string source = parseJsonField(body, "source");
    if (source.empty()) {
        source = "admin-portal";
    }
    std::string timestamp = parseJsonField(body, "timestamp");
    if (timestamp.empty()) {
        timestamp = getCurrentTimestamp();
    }
    std::string idempotencyKey = parseJsonField(body, "idempotency_key");

    if (!logAccessAttempt(db, memberId, granted, reason, timestamp, source, idempotencyKey)) {
        return jsonError("Failed to log access attempt");
    }
    return "{\"success\":true}";
}

static std::string handleMemberChanges(sqlite3* db, const std::string& query)
{
    std::string since = getQueryParam(query, "since");
    if (since.empty()) {
        since = "1970-01-01 00:00:00";
    }

    auto members = getMembersChangedSince(db, since);
    std::ostringstream oss;
    oss << "{\"success\":true,\"members\":[";
    for (size_t i = 0; i < members.size(); ++i) {
        if (i > 0) {
            oss << ",";
        }
        oss << memberToJson(members[i]);
    }
    oss << "]}";
    return oss.str();
}

static std::string handleStats(sqlite3* db)
{
    auto members = getAllMembers(db);
    size_t total = members.size();
    size_t active = 0;
    size_t inactive = 0;
    size_t pending = 0;
    for (const auto& m : members) {
        if (m.member_status == "ACTIVE") {
            ++active;
        } else if (m.member_status == "PENDING_ENROLLMENT") {
            ++pending;
        } else {
            ++inactive;
        }
    }

    std::string today = getCurrentTimestamp().substr(0, 10);
    size_t todayEntries = 0;
    for (const auto& log : getLogsByDate(db, today)) {
        if (log.granted) {
            ++todayEntries;
        }
    }

    std::ostringstream oss;
    oss << "{\"success\":true,\"stats\":{"
        << "\"total_members\":" << total << ","
        << "\"active_members\":" << active << ","
        << "\"inactive_members\":" << inactive << ","
        << "\"pending_enrollments\":" << pending << ","
        << "\"today_entries\":" << todayEntries
        << "}}";
    return oss.str();
}

struct ConnectionContext {
    std::string db_path;
    std::string api_key;
};

static void handleConnection(SocketType client, ConnectionContext context)
{
    sqlite3* db = nullptr;
    if (!openDatabase(db, context.db_path)) {
#ifdef _WIN32
        closesocket(client);
#else
        close(client);
#endif
        return;
    }

    constexpr size_t bufferSize = 8192;
    std::string requestData;
    requestData.reserve(bufferSize);
    char buffer[bufferSize];

    int received = static_cast<int>(recv(client, buffer, bufferSize, 0));
    if (received <= 0) {
        closeDatabase(db);
#ifdef _WIN32
        closesocket(client);
#else
        close(client);
#endif
        return;
    }
    requestData.append(buffer, received);

    auto headerEnd = requestData.find("\r\n\r\n");
    if (headerEnd == std::string::npos) {
        closeDatabase(db);
#ifdef _WIN32
        closesocket(client);
#else
        close(client);
#endif
        return;
    }

    HttpRequest request = parseHttpRequest(requestData);
    if (request.headers.count("content-length") > 0) {
        size_t contentLength = std::stoul(request.headers["content-length"]);
        auto bodyStart = headerEnd + 4;
        if (requestData.size() < bodyStart + contentLength) {
            size_t remaining = bodyStart + contentLength - requestData.size();
            while (remaining > 0) {
                int chunk = static_cast<int>(recv(client, buffer, std::min(bufferSize, remaining), 0));
                if (chunk <= 0) {
                    break;
                }
                requestData.append(buffer, chunk);
                remaining -= chunk;
            }
            request.body = requestData.substr(bodyStart, contentLength);
        }
    }

    std::string path = api::normalizePath(request.path);
    std::string responseBody;
    int statusCode = 200;

    auto apiKeyIt = request.headers.find("x-api-key");
    if (request.method == "OPTIONS") {
        statusCode = 204;
        responseBody = "";
    } else if (apiKeyIt == request.headers.end() || apiKeyIt->second != context.api_key) {
        statusCode = 401;
        responseBody = jsonError("Unauthorized");
    } else if (request.method == "GET" && path == api::kMembersChanges) {
        responseBody = handleMemberChanges(db, request.query);
    } else if (request.method == "GET" && path == api::kMembers) {
        responseBody = handleMemberCollection(db, request);
    } else if (request.method == "POST" && path == api::kMembers) {
        responseBody = handleMemberCollection(db, request);
        if (responseBody.find("already exists") != std::string::npos) {
            statusCode = 409;
        } else if (responseBody.find("\"success\":false") != std::string::npos) {
            statusCode = 400;
        } else {
            statusCode = 201;
        }
    } else if (request.method == "GET" && path == api::kStats) {
        responseBody = handleStats(db);
    } else if (request.method == "GET" && path == api::kEnrollmentPending) {
        responseBody = handleEnrollmentPending(db);
    } else if (request.method == "POST" && path == api::kEnrollmentStart) {
        responseBody = handleEnrollmentStart(db, request.body);
    } else if (request.method == "POST" && path == api::kEnrollmentResult) {
        responseBody = handleEnrollmentResult(db, request.body);
    } else if (request.method == "GET" && path == api::kAccessLogs) {
        responseBody = handleAccessLogs(db, request);
    } else if (request.method == "GET" && path.rfind("/access/logs/", 0) == 0) {
        std::string memberId = urlDecode(path.substr(std::string("/access/logs/").size()));
        responseBody = handleAccessLogsByMember(db, memberId);
    } else if (request.method == "POST" && path == api::kAccessLog) {
        responseBody = handleAccessLogPost(db, request.body);
        if (responseBody.find("\"success\":false") != std::string::npos) {
            statusCode = 400;
        }
    } else if (request.method == "GET" && path.rfind("/access/", 0) == 0) {
        std::string memberId = urlDecode(path.substr(std::string("/access/").size()));
        responseBody = handleAccessCheck(db, memberId);
        if (responseBody.find("\"success\":false") != std::string::npos) {
            statusCode = 404;
        }
    } else if (path.rfind("/members/", 0) == 0) {
        std::string memberId = urlDecode(path.substr(std::string("/members/").size()));
        responseBody = handleMemberItem(db, request, memberId);
        if (responseBody.find("\"success\":false") != std::string::npos) {
            statusCode = (responseBody.find("not found") != std::string::npos) ? 404 : 400;
        }
    } else {
        statusCode = 404;
        responseBody = jsonError("Endpoint not found");
    }

    sendResponse(client, statusCode, responseBody);
    closeDatabase(db);

#ifdef _WIN32
    closesocket(client);
#else
    close(client);
#endif
}

RestServer::RestServer(Config config)
    : config_(std::move(config))
{
}

RestServer::~RestServer()
{
    requestStop();
    waitUntilStopped();
}

bool RestServer::initializeDatabaseSchema()
{
    sqlite3* db = nullptr;
    if (!openDatabase(db, config_.db_path)) {
        return false;
    }

    bool ok = createMembersTable(db) &&
              createSyncStatusTable(db) &&
              initializeSyncStatus(db) &&
              createAccessLogsTable(db);
    closeDatabase(db);
    return ok;
}

bool RestServer::startInBackground()
{
    if (running_.load()) {
        return true;
    }

    stop_requested_.store(false);
    server_thread_ = std::thread(&RestServer::serverLoop, this);
    return true;
}

void RestServer::requestStop()
{
    stop_requested_.store(true);
#ifdef _WIN32
    SocketType sock = static_cast<SocketType>(server_socket_);
    if (sock != INVALID_SOCKET) {
        closesocket(sock);
    }
#else
    if (server_socket_ >= 0) {
        close(server_socket_);
    }
#endif
}

void RestServer::waitUntilStopped()
{
    if (server_thread_.joinable()) {
        server_thread_.join();
    }
}

void RestServer::serverLoop()
{
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed" << std::endl;
        return;
    }
#endif

    SocketType serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
#ifdef _WIN32
    server_socket_ = static_cast<uintptr_t>(serverSocket);
    if (serverSocket == INVALID_SOCKET) {
#else
    server_socket_ = serverSocket;
    if (serverSocket < 0) {
#endif
        std::cerr << "Failed to create socket" << std::endl;
        return;
    }

    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    serverAddress.sin_port = htons(static_cast<uint16_t>(config_.port));

    int reuse = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));

    if (bind(serverSocket, reinterpret_cast<sockaddr*>(&serverAddress), sizeof(serverAddress)) < 0) {
        std::cerr << "Failed to bind socket on port " << config_.port << std::endl;
#ifdef _WIN32
        closesocket(serverSocket);
        WSACleanup();
#else
        close(serverSocket);
#endif
        return;
    }

    if (listen(serverSocket, 10) < 0) {
        std::cerr << "Failed to listen on socket" << std::endl;
#ifdef _WIN32
        closesocket(serverSocket);
        WSACleanup();
#else
        close(serverSocket);
#endif
        return;
    }

    running_.store(true);
    std::cout << "REST server running on http://localhost:" << config_.port << std::endl;

    ConnectionContext context{config_.db_path, config_.api_key};

    while (!stop_requested_.load()) {
        sockaddr_in clientAddress{};
        socklen_t clientLen = sizeof(clientAddress);
        SocketType clientSocket = accept(serverSocket, reinterpret_cast<sockaddr*>(&clientAddress), &clientLen);
#ifdef _WIN32
        if (clientSocket == INVALID_SOCKET) {
#else
        if (clientSocket < 0) {
#endif
            if (stop_requested_.load()) {
                break;
            }
            continue;
        }

        std::thread(handleConnection, clientSocket, context).detach();
    }

#ifdef _WIN32
    closesocket(serverSocket);
    WSACleanup();
#else
    close(serverSocket);
#endif

    running_.store(false);
}
