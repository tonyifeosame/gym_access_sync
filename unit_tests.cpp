#include <cassert>
#include <iostream>
#include "api_client.h"
#include "database.h"
#include "fingerprint_service.h"
#include "mock_fingerprint_scanner.h"
#include "door_controller.h"
#include "mock_relay.h"
#include "validation.h"

static bool runDatabaseTests()
{
    sqlite3* db = nullptr;
    assert(openDatabase(db, ":memory:"));
    assert(createMembersTable(db));
    assert(createSyncStatusTable(db));
    assert(initializeSyncStatus(db));
    assert(createAccessLogsTable(db));

    Member member{"100", "Test User", "template-100", "ACTIVE", "2099-12-31", "2026-06-07 00:00:00"};
    assert(insertMember(db, member));

    auto loaded = getMemberById(db, "100");
    assert(loaded.has_value());
    assert(loaded->member_id == "100");
    assert(loaded->member_status == "ACTIVE");

    auto activeMembers = getMembersByStatus(db, "ACTIVE");
    assert(activeMembers.size() == 1);
    assert(activeMembers[0].member_id == "100");

    assert(logAccessAttempt(db, "100", true, "Test", "2026-06-07 00:00:00", "UNIT_TEST", ""));
    auto logs = getAccessLogs(db);
    assert(logs.size() == 1);
    assert(logs[0].member_id == "100");
    assert(logs[0].granted);
    assert(logs[0].source == "UNIT_TEST");

    closeDatabase(db);
    return true;
}

static bool runApiClientTests()
{
    sqlite3* db = nullptr;
    assert(openDatabase(db, ":memory:"));
    assert(createMembersTable(db));
    assert(createSyncStatusTable(db));
    assert(initializeSyncStatus(db));
    assert(createAccessLogsTable(db));

    ApiConfig config;
    config.api_url = "http://127.0.0.1:9999";
    config.api_key = "test-key";
    config.offline_mode = true;
    config.scanner.ip = "";
    ApiClient apiClient(config);

    Member member{"101", "Api Test", "", "ACTIVE", "2099-12-31", "2026-06-07 00:00:00"};
    assert(insertMember(db, member));

    auto allMembers = apiClient.fetchAllMembers(db);
    assert(allMembers.size() == 1);
    assert(allMembers[0].member_id == "101");

    assert(apiClient.startEnrollment("101", db));
    auto pending = apiClient.fetchPendingEnrollments(db);
    assert(pending.size() == 1);
    assert(pending[0] == "101");

    assert(apiClient.submitEnrollmentResult("101", "template-101", db));
    auto updated = getMemberById(db, "101");
    assert(updated.has_value());
    assert(updated->member_status == "ACTIVE");
    assert(updated->member_fingerprint_template == "template-101");

    assert(apiClient.postAccessLog("101", true, "Unit test check-in", "UNIT_TEST", db));
    auto logs = getAccessLogs(db);
    assert(logs.size() == 1);
    assert(logs[0].member_id == "101");
    assert(logs[0].granted);
    assert(logs[0].source == "UNIT_TEST");

    closeDatabase(db);
    return true;
}

static bool runFingerprintServiceTests()
{
    sqlite3* db = nullptr;
    assert(openDatabase(db, ":memory:"));
    assert(createMembersTable(db));
    assert(createSyncStatusTable(db));
    assert(initializeSyncStatus(db));

    ApiConfig config;
    config.api_url = "http://127.0.0.1:9999";
    config.api_key = "test-key";
    config.offline_mode = true;
    ApiClient apiClient(config);

    Member member{"102", "Enroll Test", "", "ACTIVE", "2099-12-31", "2026-06-07 00:00:00"};
    assert(insertMember(db, member));

    MockFingerprintScanner scanner;
    FingerprintService service(apiClient, db, &scanner);

    assert(service.startEnrollment("102"));
    assert(service.verifyFingerprint());

    auto updated = getMemberById(db, "102");
    assert(updated.has_value());
    assert(updated->member_status == "ACTIVE");
    assert(!updated->member_fingerprint_template.empty());

    closeDatabase(db);
    return true;
}

static bool runDoorControllerTests()
{
    MockRelay relay;
    DoorController controller(&relay);
    assert(controller.unlock(0));
    return true;
}

static bool runValidationTests()
{
    Member pending{"200", "Pending User", "", "PENDING_ENROLLMENT", "", "2026-06-09 00:00:00"};
    assert(validateMember(pending));

    Member active{"201", "Active User", "template-201", "ACTIVE", "2099-12-31", "2026-06-09 00:00:00"};
    assert(validateMember(active));

    Member missingFingerprint{"202", "Bad User", "", "ACTIVE", "2099-12-31", "2026-06-09 00:00:00"};
    assert(!validateMember(missingFingerprint));
    return true;
}

int main()
{
    std::cout << "Running unit tests..." << std::endl;
    assert(runDatabaseTests());
    std::cout << "Database tests passed." << std::endl;
    assert(runApiClientTests());
    std::cout << "ApiClient tests passed." << std::endl;
    assert(runFingerprintServiceTests());
    std::cout << "FingerprintService tests passed." << std::endl;
    assert(runDoorControllerTests());
    std::cout << "DoorController tests passed." << std::endl;
    assert(runValidationTests());
    std::cout << "Validation tests passed." << std::endl;
    std::cout << "All unit tests passed." << std::endl;
    return 0;
}
