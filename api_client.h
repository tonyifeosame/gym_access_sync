#pragma once

#include <optional>
#include <string>
#include <vector>
#include <sqlite3.h>
#include "member.h"

struct ScannerConfig {
    std::string ip;
    int port = 4370;
    int timeout = 30;
};

struct ApiConfig {
    std::string api_url;
    std::string api_key;
    int sync_interval_seconds = 60;
    std::string site_name;
    bool offline_mode = false;
    ScannerConfig scanner;
};

bool loadConfig(const std::string& configPath, ApiConfig& config);

struct AccessResult {
    std::string member_id;
    std::string status;
    bool granted = false;
    std::string message;
    bool server_available = true;
    std::string source;
};

class ApiClient {
public:
    explicit ApiClient(const ApiConfig& config);

    std::vector<Member> fetchAllMembers(sqlite3* localDb = nullptr);
    std::vector<Member> fetchChangedMembers(const std::string& lastSyncTime, sqlite3* localDb = nullptr);
    std::optional<Member> fetchMember(const std::string& member_id);
    std::optional<AccessResult> fetchAccess(const std::string& member_id, sqlite3* localDb = nullptr);

    bool startEnrollment(const std::string& member_id, sqlite3* localDb = nullptr);
    std::vector<std::string> fetchPendingEnrollments(sqlite3* localDb = nullptr);
    bool submitEnrollmentResult(const std::string& member_id, const std::string& fingerprintTemplate, sqlite3* localDb = nullptr);

private:
    ApiConfig config_;
};
