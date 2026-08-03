#include "api_client.h"
#include "database.h"
#include "rest_server.h"

#include <cassert>
#include <chrono>
#include <cstdio>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
using SocketType = SOCKET;
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/time.h>
using SocketType = int;
#endif

#define ASSERT_LOG(condition) \
    do { \
        assert(condition); \
    } while (0)

static const char* kTestDb = "test_checkin.db";
static const int kTestPort = 18081;

static void cleanupTestDb()
{
    std::remove(kTestDb);
}

static bool initializeSockets()
{
#ifdef _WIN32
    WSADATA wsaData;
    return WSAStartup(MAKEWORD(2, 2), &wsaData) == 0;
#else
    return true;
#endif
}

static void shutdownSockets()
{
#ifdef _WIN32
    WSACleanup();
#endif
}

static std::string sendHttpRequest(const std::string& method, const std::string& path, const std::string& body, const std::string& apiKey, int port)
{
    if (!initializeSockets()) {
        return {};
    }

    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    addrinfo* result = nullptr;
    if (getaddrinfo("127.0.0.1", std::to_string(port).c_str(), &hints, &result) != 0) {
        shutdownSockets();
        return {};
    }

    SocketType sock = INVALID_SOCKET;
    for (addrinfo* ptr = result; ptr != nullptr; ptr = ptr->ai_next) {
        sock = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
        if (sock == INVALID_SOCKET) {
            continue;
        }
        if (connect(sock, ptr->ai_addr, static_cast<int>(ptr->ai_addrlen)) == 0) {
            break;
        }
#ifdef _WIN32
        closesocket(sock);
#else
        close(sock);
#endif
        sock = INVALID_SOCKET;
    }

    freeaddrinfo(result);
    if (sock == INVALID_SOCKET) {
        shutdownSockets();
        return {};
    }

#ifdef _WIN32
    DWORD timeoutMs = 2000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeoutMs), sizeof(timeoutMs));
#else
    timeval timeout{2, 0};
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
#endif

    std::ostringstream request;
    request << method << " " << path << " HTTP/1.1\r\n"
            << "Host: 127.0.0.1\r\n"
            << "X-API-Key: " << apiKey << "\r\n"
            << "Content-Type: application/json\r\n"
            << "Content-Length: " << body.size() << "\r\n"
            << "Connection: close\r\n"
            << "\r\n"
            << body;

    std::string requestText = request.str();
    send(sock, requestText.c_str(), static_cast<int>(requestText.size()), 0);

    std::string response;
    char buffer[4096];
    int received = 0;
    for (int attempt = 0; attempt < 20; ++attempt) {
        received = recv(sock, buffer, sizeof(buffer), 0);
        if (received > 0) {
            response.append(buffer, received);
        } else {
            break;
        }
    }

#ifdef _WIN32
    closesocket(sock);
#else
    close(sock);
#endif
    shutdownSockets();
    return response;
}

static void testBasicCheckIn()
{

    sqlite3* db = nullptr;
    ASSERT_LOG(openDatabase(db, kTestDb));
    ASSERT_LOG(createMembersTable(db));
    ASSERT_LOG(createAccessLogsTable(db));

    Member active{"001", "Test Member", "fp-001", "ACTIVE", "2099-12-31", "2026-06-09 10:00:00"};
    Member inactive{"002", "Inactive Member", "fp-002", "INACTIVE", "2099-12-31", "2026-06-09 10:00:00"};
    ASSERT_LOG(insertMember(db, active));
    ASSERT_LOG(insertMember(db, inactive));

    ApiConfig config;
    config.api_url = "http://127.0.0.1:" + std::to_string(kTestPort);
    config.api_key = "test-key";
    config.offline_mode = true;
    ApiClient apiClient(config);

    auto granted = apiClient.checkIn("001", db);
    ASSERT_LOG(granted.has_value());
    ASSERT_LOG(granted->granted);

    auto denied = apiClient.checkIn("002", db);
    ASSERT_LOG(denied.has_value());
    ASSERT_LOG(!denied->granted);

    auto logs = getAccessLogs(db);
    ASSERT_LOG(logs.size() == 2);
    ASSERT_LOG(logs[0].member_id == "002" || logs[0].member_id == "001");
    ASSERT_LOG(logs[0].source == "terminal-check-in" || logs[1].source == "terminal-check-in");

    closeDatabase(db);
}

static void testDuplicatePostRequests()
{

    sqlite3* db = nullptr;
    ASSERT_LOG(openDatabase(db, kTestDb));
    ASSERT_LOG(createMembersTable(db));
    ASSERT_LOG(createAccessLogsTable(db));

    Member member{"003", "Duplicate Test", "fp-003", "ACTIVE", "2099-12-31", "2026-06-09 10:00:00"};
    ASSERT_LOG(insertMember(db, member));

    std::string idempotencyKey = "test-idempotency-key-12345";
    std::string payload = "{\"member_id\":\"003\",\"granted\":true,\"reason\":\"Test\",\"source\":\"test-duplicate\",\"timestamp\":\"2026-06-09 10:00:00\",\"idempotency_key\":\"" + idempotencyKey + "\"}";

    std::string firstResponse = sendHttpRequest("POST", "/access/log", payload, "test-key", kTestPort);
    ASSERT_LOG(firstResponse.find("\"success\":true") != std::string::npos);

    auto logsAfterFirst = getAccessLogs(db);
    ASSERT_LOG(logsAfterFirst.size() == 1);

    std::string secondResponse = sendHttpRequest("POST", "/access/log", payload, "test-key", kTestPort);
    ASSERT_LOG(secondResponse.find("\"success\":true") != std::string::npos);

    auto logsAfterSecond = getAccessLogs(db);
    ASSERT_LOG(logsAfterSecond.size() == 1);

    closeDatabase(db);
}

static void testFallbackScenario()
{

    sqlite3* db = nullptr;

    ASSERT_LOG(openDatabase(db, kTestDb));

    ASSERT_LOG(createMembersTable(db));

    ASSERT_LOG(createAccessLogsTable(db));

    Member member{
        "004",
        "Fallback Test",
        "fp-004",
        "ACTIVE",
        "2099-12-31",
        "2026-06-09 10:00:00"
    };

    ASSERT_LOG(insertMember(db, member));

    ApiConfig config;
    config.api_url = "http://127.0.0.1:9999";
    config.api_key = "test-key";
    config.offline_mode = true;

    ApiClient apiClient(config);

    auto result = apiClient.checkIn("004", db);

    ASSERT_LOG(result.has_value());

    ASSERT_LOG(result->granted);

    auto logs = getAccessLogs(db);

    ASSERT_LOG(logs.size() == 1);

    ASSERT_LOG(logs[0].member_id == "004");

    ASSERT_LOG(logs[0].source == "terminal-check-in");

    closeDatabase(db);
}

static void testRetryAfterTimeout()
{
    sqlite3* db = nullptr;
    ASSERT_LOG(openDatabase(db, kTestDb));

    ASSERT_LOG(createMembersTable(db));

    ASSERT_LOG(createAccessLogsTable(db));

    Member member{"005", "Retry Test", "fp-005", "ACTIVE", "2099-12-31", "2026-06-09 10:00:00"};
    ASSERT_LOG(insertMember(db, member));

    ApiConfig config;
    config.api_url = "http://127.0.0.1:" + std::to_string(kTestPort);
    config.api_key = "test-key";
    config.offline_mode = false;
    ApiClient apiClient(config);

    auto firstResult = apiClient.checkIn("005", db);
    ASSERT_LOG(firstResult.has_value());
    ASSERT_LOG(firstResult->granted);

    auto secondResult = apiClient.checkIn("005", db);
    ASSERT_LOG(secondResult.has_value());
    ASSERT_LOG(secondResult->granted);

    auto logs = getAccessLogs(db);

    ASSERT_LOG(logs.size() == 2);

    ASSERT_LOG(logs[0].member_id == "005");

    ASSERT_LOG(logs[1].member_id == "005");

    ASSERT_LOG(logs[0].idempotency_key != logs[1].idempotency_key);

    closeDatabase(db);
}

static void testDeviceHeartbeatFlow()
{

    sqlite3* db = nullptr;
    ASSERT_LOG(openDatabase(db, kTestDb));
    ASSERT_LOG(createMembersTable(db));
    ASSERT_LOG(createSyncStatusTable(db));
    ASSERT_LOG(initializeSyncStatus(db));
    ASSERT_LOG(createAccessLogsTable(db));
    ASSERT_LOG(createDeviceCommandQueueTable(db));
    ASSERT_LOG(createDeviceStatusTable(db));

    DeviceCommand command;
    command.device_id = "esp32-test";
    command.command_type = "ENROLL";
    command.payload = "member-300";
    command.status = "PENDING";
    ASSERT_LOG(queueDeviceCommand(db, command));

    const std::string heartbeatBody = "{\"device_id\":\"esp32-test\",\"status\":\"ONLINE\",\"details\":\"heartbeat\"}";
    std::string heartbeatResponse = sendHttpRequest("POST", "/device/heartbeat", heartbeatBody, "test-key", kTestPort);
    ASSERT_LOG(heartbeatResponse.find("\"success\":true") != std::string::npos);

    std::string statusResponse = sendHttpRequest("GET", "/device/status?device_id=esp32-test", "", "test-key", kTestPort);
    ASSERT_LOG(statusResponse.find("\"status\":\"ONLINE\"") != std::string::npos);

    std::string commandsResponse = sendHttpRequest("GET", "/device/commands/esp32-test", "", "test-key", kTestPort);
    ASSERT_LOG(commandsResponse.find("\"command_type\":\"ENROLL\"") != std::string::npos);

    closeDatabase(db);
}

int main()
{
    cleanupTestDb();

    RestServer::Config config;
    config.db_path = kTestDb;
    config.api_key = "test-key";
    config.port = kTestPort;

    RestServer server(config);

    ASSERT_LOG(server.initializeDatabaseSchema());

    ASSERT_LOG(server.startInBackground());
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    testBasicCheckIn();

    server.requestStop();

    server.waitUntilStopped();

    // Duplicate POST
    {
        cleanupTestDb();

        RestServer::Config config;
        config.db_path = kTestDb;
        config.api_key = "test-key";
        config.port = kTestPort;

        RestServer server(config);
        ASSERT_LOG(server.initializeDatabaseSchema());
        ASSERT_LOG(server.startInBackground());
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        testDuplicatePostRequests();

        server.requestStop();
        server.waitUntilStopped();
        cleanupTestDb();
    }

    // Fallback
    {
        cleanupTestDb();

        RestServer::Config config;
        config.db_path = kTestDb;
        config.api_key = "test-key";
        config.port = kTestPort;

        RestServer server(config);
        ASSERT_LOG(server.initializeDatabaseSchema());
        ASSERT_LOG(server.startInBackground());
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        testFallbackScenario();

        server.requestStop();
        server.waitUntilStopped();
        cleanupTestDb();
    }

    // Retry
    {
        cleanupTestDb();

        RestServer::Config config;
        config.db_path = kTestDb;
        config.api_key = "test-key";
        config.port = kTestPort;

        RestServer server(config);
        ASSERT_LOG(server.initializeDatabaseSchema());
        ASSERT_LOG(server.startInBackground());
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        testRetryAfterTimeout();

        server.requestStop();
        server.waitUntilStopped();
        cleanupTestDb();
    }

    // Device heartbeat
    {
        cleanupTestDb();

        RestServer::Config config;
        config.db_path = kTestDb;
        config.api_key = "test-key";
        config.port = kTestPort;

        RestServer server(config);
        ASSERT_LOG(server.initializeDatabaseSchema());
        ASSERT_LOG(server.startInBackground());
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        testDeviceHeartbeatFlow();

        server.requestStop();
        server.waitUntilStopped();
        cleanupTestDb();
    }

    std::cout << "All integration tests passed." << std::endl;
    return 0;
}