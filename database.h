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
};

bool createAccessLogsTable(sqlite3* db);
std::vector<AccessLog> getAccessLogs(sqlite3* db);
std::vector<AccessLog> getLogsByMember(sqlite3* db, const std::string& member_id);
std::vector<AccessLog> getLogsByDate(sqlite3* db, const std::string& date);
bool logAccessAttempt(sqlite3* db, const std::string& member_id, bool granted, const std::string& reason, const std::string& timestamp, const std::string& source);
void closeDatabase(sqlite3* db);
