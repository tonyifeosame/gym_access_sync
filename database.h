#pragma once

#include <optional>
#include <string>
#include <vector>
#include <sqlite3.h>
#include "member.h"

bool openDatabase(sqlite3*& db, const std::string& path);
bool createMembersTable(sqlite3* db);
bool insertMember(sqlite3* db, const Member& member);
bool updateMember(sqlite3* db, const Member& member);
bool deleteMember(sqlite3* db, const std::string& member_id);
bool markMemberInactive(sqlite3* db, const std::string& member_id, const std::string& last_updated);
bool memberExists(sqlite3* db, const std::string& member_id);
std::optional<Member> getMemberById(sqlite3* db, const std::string& member_id);
std::vector<Member> getAllMembers(sqlite3* db);
std::vector<Member> getMembersChangedSince(sqlite3* db, const std::string& since);
std::vector<Member> getMembersByStatus(sqlite3* db, const std::string& status);
std::vector<std::string> getAllMemberIds(sqlite3* db);
bool createSyncStatusTable(sqlite3* db);
bool initializeSyncStatus(sqlite3* db);
bool updateLastSyncTime(sqlite3* db, const std::string& timestamp);
std::optional<std::string> getLastSyncTime(sqlite3* db);

struct AccessLog {
    int id;
    std::string member_id;
    bool granted;
    std::string reason;
    std::string timestamp;
    std::string source;
    std::string idempotency_key;
};

struct DeviceCommand {
    int id = 0;
    std::string device_id;
    std::string command_type;
    std::string payload;
    std::string status;
    std::string created_at;
    std::string updated_at;
};

struct DeviceStatus {
    std::string device_id;
    std::string status;
    std::string last_seen;
    std::string details;
    std::string updated_at;
};

bool createAccessLogsTable(sqlite3* db);
std::vector<AccessLog> getAccessLogs(sqlite3* db);
std::vector<AccessLog> getLogsByMember(sqlite3* db, const std::string& member_id);
std::vector<AccessLog> getLogsByDate(sqlite3* db, const std::string& date);
bool logAccessAttempt(sqlite3* db, const std::string& member_id, bool granted, const std::string& reason, const std::string& timestamp, const std::string& source, const std::string& idempotency_key = "");
bool createDeviceCommandQueueTable(sqlite3* db);
bool createDeviceStatusTable(sqlite3* db);
bool queueDeviceCommand(sqlite3* db, const DeviceCommand& command);
std::vector<DeviceCommand> getDeviceCommands(sqlite3* db, const std::string& device_id, bool pending_only = true);
bool markDeviceCommandCompleted(sqlite3* db, int command_id, const std::string& status = "COMPLETED");
bool updateDeviceStatus(sqlite3* db, const std::string& device_id, const std::string& status, const std::string& last_seen, const std::string& details);
std::optional<DeviceStatus> getDeviceStatus(sqlite3* db, const std::string& device_id);
std::vector<DeviceStatus> getAllDeviceStatuses(sqlite3* db);
void closeDatabase(sqlite3* db);
