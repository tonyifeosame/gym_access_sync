#include <iostream>
#include <vector>
#include <optional>
#include <ctime>
#include <chrono>
#include <iomanip>
#include "member.h"
#include "validation.h"
#include "database.h"
#include "sync.h"
#include "api_client.h"
#include "door_controller.h"
#include "mock_relay.h"
#include "fingerprint_service.h"
#include "mock_fingerprint_scanner.h"
#include "zkteco_scanner.h"

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

void simulateScan(sqlite3* db, ApiClient& apiClient, DoorController& doorController, const std::string& member_id)
{
    std::cout << "Simulating scan for member " << member_id << std::endl;
    auto accessOpt = apiClient.fetchAccess(member_id, db);
    if (!accessOpt) {
        std::cout << "Access Denied: unable to evaluate access" << std::endl;
        logError("Access Denied: " + member_id + " - unable to evaluate access");
        logAccessAttempt(db, member_id, false, "Unable to evaluate access", getCurrentTimestamp(), "UNKNOWN");
        return;
    }

    auto access = *accessOpt;
    std::cout << access.message << std::endl;
    logError((access.granted ? "Access Granted: " : "Access Denied: ") + member_id + " - " + access.message + " (source=" + access.source + ")");
    logAccessAttempt(db, member_id, access.granted, access.message, getCurrentTimestamp(), access.source);

    if (access.granted) {
        doorController.unlock(5);
    }
}

int main()
{
    ApiConfig config;
    if (!loadConfig("config.json", config)) {
        std::cout << "Failed to load config.json" << std::endl;
        return 1;
    }

    std::cout << "Using API URL: " << config.api_url << std::endl;
    std::cout << "Sync interval: " << config.sync_interval_seconds << " seconds" << std::endl;
    std::cout << "Scanner IP: " << config.scanner.ip << std::endl;
    std::cout << "Scanner port: " << config.scanner.port << std::endl;
    std::cout << "Enrollment timeout: " << config.scanner.timeout << "s" << std::endl;

    sqlite3* db = nullptr;
    if (!openDatabase(db, "members.db")) {
        std::cout << "Failed to open database" << std::endl;
        return 1;
    }

    if (!createMembersTable(db) || !createSyncStatusTable(db) || !initializeSyncStatus(db) || !createAccessLogsTable(db)) {
        std::cout << "Failed to initialize database" << std::endl;
        closeDatabase(db);
        return 1;
    }

    auto lastSync = getLastSyncTime(db).value_or("1970-01-01 00:00:00");
    std::cout << "Last successful sync: " << lastSync << std::endl;

    ApiClient apiClient(config);
    if (lastSync == "1970-01-01 00:00:00") {
        auto members = apiClient.fetchAllMembers(db);
        if (!validateDuplicateIds(members)) {
            std::cout << "Duplicate IDs found in cloud member list. Synchronization aborted." << std::endl;
            closeDatabase(db);
            return 1;
        }

        int validCount = 0;
        int invalidCount = 0;
        for (const auto& member : members) {
            if (validateMember(member)) {
                validCount++;
            } else {
                invalidCount++;
            }
        }

        std::cout << "Members size: " << members.size() << std::endl;
        std::cout << "Valid Members: " << validCount << std::endl;
        std::cout << "Invalid Members: " << invalidCount << std::endl;
        std::cout << "Starting full synchronization..." << std::endl;

        synchronizeMembers(db, members);
        synchronizeDeletedMembers(db, members);
    } else {
        auto changedMembers = apiClient.fetchChangedMembers(lastSync, db);
        std::cout << "Found " << changedMembers.size() << " changed members since " << lastSync << std::endl;
        std::cout << "Starting incremental synchronization..." << std::endl;

        if (!validateDuplicateIds(changedMembers)) {
            std::cout << "Duplicate IDs found in changed member list. Synchronization aborted." << std::endl;
            closeDatabase(db);
            return 1;
        }

        synchronizeMembers(db, changedMembers);
        std::cout << "Skipping missing-member cleanup for incremental sync." << std::endl;
    }

    if (!recordLastSync(db)) {
        std::cout << "Failed to record sync timestamp" << std::endl;
    }

    auto newLastSync = getLastSyncTime(db).value_or("unknown");
    std::cout << "Synchronization completed." << std::endl;
    std::cout << "Last successful sync: " << newLastSync << std::endl;

    MockFingerprintScanner mockScanner;
    ZKTecoScanner zktecoScanner(config.scanner.ip, config.scanner.port, config.scanner.timeout);
    FingerprintScannerInterface* scanner = &mockScanner;
    std::string scannerStatus = "MOCK";
    if (!config.scanner.ip.empty()) {
        std::cout << "Using ZKTeco scanner stub at " << config.scanner.ip << ":" << config.scanner.port << std::endl;
        if (zktecoScanner.connect()) {
            scannerStatus = "ONLINE";
        } else {
            scannerStatus = "OFFLINE";
        }
        std::cout << "Scanner Status: " << scannerStatus << std::endl;
        scanner = &zktecoScanner;
    } else {
        std::cout << "Scanner Status: " << scannerStatus << std::endl;
    }

    FingerprintService fingerprintService(apiClient, db, scanner);
    if (fingerprintService.startEnrollment("005")) {
        std::cout << "Enrollment started for member 005" << std::endl;
        if (fingerprintService.verifyFingerprint()) {
            std::cout << "Fingerprint enrollment completed for member 005" << std::endl;
        }
    }

    MockRelay mockRelay;
    DoorController doorController(&mockRelay);
    simulateScan(db, apiClient, doorController, "001");
    simulateScan(db, apiClient, doorController, "002");
    simulateScan(db, apiClient, doorController, "005");

    closeDatabase(db);
    return 0;
}
