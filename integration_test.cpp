#include "api_client.h"
#include "database.h"
#include "rest_server.h"

#include <cassert>
#include <chrono>
#include <cstdio>
#include <iostream>
#include <thread>

static const char* kTestDb = "test_checkin.db";
static const int kTestPort = 18081;

static void cleanupTestDb()
{
    std::remove(kTestDb);
}

static void testBasicCheckIn()
{
    std::cout << "Testing basic check-in logging..." << std::endl;
    
    sqlite3* db = nullptr;
    assert(openDatabase(db, kTestDb));
    assert(createMembersTable(db));
    assert(createAccessLogsTable(db));

    Member active{"001", "Test Member", "fp-001", "ACTIVE", "2099-12-31", "2026-06-09 10:00:00"};
    Member inactive{"002", "Inactive Member", "fp-002", "INACTIVE", "2099-12-31", "2026-06-09 10:00:00"};
    assert(insertMember(db, active));
    assert(insertMember(db, inactive));

    ApiConfig config;
    config.api_url = "http://127.0.0.1:" + std::to_string(kTestPort);
    config.api_key = "test-key";
    config.offline_mode = true;
    ApiClient apiClient(config);

    auto granted = apiClient.checkIn("001", db);
    assert(granted.has_value());
    assert(granted->granted);

    auto denied = apiClient.checkIn("002", db);
    assert(denied.has_value());
    assert(!denied->granted);

    auto logs = getAccessLogs(db);
    assert(logs.size() == 2);
    assert(logs[0].member_id == "002" || logs[0].member_id == "001");
    assert(logs[0].source == "terminal-check-in" || logs[1].source == "terminal-check-in");

    closeDatabase(db);
    std::cout << "Basic check-in logging test passed." << std::endl;
}

static void testDuplicatePostRequests()
{
    std::cout << "Testing duplicate POST request prevention..." << std::endl;
    
    sqlite3* db = nullptr;
    assert(openDatabase(db, kTestDb));
    assert(createMembersTable(db));
    assert(createAccessLogsTable(db));

    Member member{"003", "Duplicate Test", "fp-003", "ACTIVE", "2099-12-31", "2026-06-09 10:00:00"};
    assert(insertMember(db, member));

    ApiConfig config;
    config.api_url = "http://127.0.0.1:" + std::to_string(kTestPort);
    config.api_key = "test-key";
    config.offline_mode = false;
    ApiClient apiClient(config);

    std::string idempotencyKey = "test-idempotency-key-12345";
    
    // Manually construct POST request with idempotency key
    std::string url = config.api_url + "/access/log";
    std::string payload = "{\"member_id\":\"003\",\"granted\":true,\"reason\":\"Test\",\"source\":\"test-duplicate\",\"timestamp\":\"2026-06-09 10:00:00\",\"idempotency_key\":\"" + idempotencyKey + "\"}";
    
    // First POST should succeed
    bool firstPost = apiClient.postAccessLog("003", true, "Test", "test-duplicate", db);
    assert(firstPost);
    
    auto logsAfterFirst = getAccessLogs(db);
    assert(logsAfterFirst.size() == 1);
    
    // Second POST with same idempotency key should not create duplicate
    // (This simulates a retry scenario)
    bool secondPost = apiClient.postAccessLog("003", true, "Test", "test-duplicate", db);
    assert(secondPost);
    
    auto logsAfterSecond = getAccessLogs(db);
    // Should still be 1 log entry, not 2
    assert(logsAfterSecond.size() == 1);

    closeDatabase(db);
    std::cout << "Duplicate POST request prevention test passed." << std::endl;
}

static void testFallbackScenario()
{
    std::cout << "Testing fallback scenario..." << std::endl;
    
    sqlite3* db = nullptr;
    assert(openDatabase(db, kTestDb));
    assert(createMembersTable(db));
    assert(createAccessLogsTable(db));

    Member member{"004", "Fallback Test", "fp-004", "ACTIVE", "2099-12-31", "2026-06-09 10:00:00"};
    assert(insertMember(db, member));

    ApiConfig config;
    config.api_url = "http://127.0.0.1:9999"; // Non-existent server
    config.api_key = "test-key";
    config.offline_mode = true;
    ApiClient apiClient(config);

    // checkIn should fallback to local logging without creating duplicates
    auto result = apiClient.checkIn("004", db);
    assert(result.has_value());
    assert(result->granted);

    auto logs = getAccessLogs(db);
    assert(logs.size() == 1);
    assert(logs[0].member_id == "004");
    assert(logs[0].source == "terminal-check-in");

    closeDatabase(db);
    std::cout << "Fallback scenario test passed." << std::endl;
}

static void testRetryAfterTimeout()
{
    std::cout << "Testing retry after timeout..." << std::endl;
    
    sqlite3* db = nullptr;
    assert(openDatabase(db, kTestDb));
    assert(createMembersTable(db));
    assert(createAccessLogsTable(db));

    Member member{"005", "Retry Test", "fp-005", "ACTIVE", "2099-12-31", "2026-06-09 10:00:00"};
    assert(insertMember(db, member));

    ApiConfig config;
    config.api_url = "http://127.0.0.1:" + std::to_string(kTestPort);
    config.api_key = "test-key";
    config.offline_mode = false;
    ApiClient apiClient(config);

    // Simulate retry by calling checkIn twice
    auto firstResult = apiClient.checkIn("005", db);
    assert(firstResult.has_value());
    assert(firstResult->granted);

    auto secondResult = apiClient.checkIn("005", db);
    assert(secondResult.has_value());
    assert(secondResult->granted);

    auto logs = getAccessLogs(db);
    // Should have 2 separate entries (different idempotency keys)
    assert(logs.size() == 2);
    assert(logs[0].member_id == "005");
    assert(logs[1].member_id == "005");
    // Verify different idempotency keys
    assert(logs[0].idempotency_key != logs[1].idempotency_key);

    closeDatabase(db);
    std::cout << "Retry after timeout test passed." << std::endl;
}

int main()
{
    std::cout << "Running integration tests..." << std::endl;
    cleanupTestDb();

    RestServer::Config serverConfig;
    serverConfig.db_path = kTestDb;
    serverConfig.api_key = "test-key";
    serverConfig.port = kTestPort;

    RestServer server(serverConfig);
    assert(server.initializeDatabaseSchema());
    assert(server.startInBackground());
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    testBasicCheckIn();
    cleanupTestDb();
    server.initializeDatabaseSchema();
    
    testDuplicatePostRequests();
    cleanupTestDb();
    server.initializeDatabaseSchema();
    
    testFallbackScenario();
    cleanupTestDb();
    server.initializeDatabaseSchema();
    
    testRetryAfterTimeout();

    server.requestStop();
    server.waitUntilStopped();
    cleanupTestDb();
    std::cout << "All integration tests passed." << std::endl;
    return 0;
}
