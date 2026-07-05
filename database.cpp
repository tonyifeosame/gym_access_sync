#include "database.h"
#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>

static std::string getCurrentTimestamp()
{
    auto now = std::chrono::system_clock::now();
    std::time_t tt = std::chrono::system_clock::to_time_t(now);
    std::tm local_tm{};
#if defined(_WIN32) || defined(_WIN64)
    localtime_s(&local_tm, &tt);
#else
    localtime_r(&tt, &local_tm);
#endif
    std::ostringstream oss;
    oss << std::put_time(&local_tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

bool openDatabase(sqlite3*& db, const std::string& path)
{
    return sqlite3_open(path.c_str(), &db) == SQLITE_OK;
}

bool createMembersTable(sqlite3* db)
{
    const char* sql =
        "CREATE TABLE IF NOT EXISTS members ("
        "member_id TEXT PRIMARY KEY, "
        "member_name TEXT, "
        "member_fingerprint_template TEXT, "
        "member_status TEXT, "
        "member_expiring_date TEXT, "
        "last_updated TEXT" 
        ");";

    return sqlite3_exec(db, sql, nullptr, nullptr, nullptr) == SQLITE_OK;
}

bool createSyncStatusTable(sqlite3* db)
{
    const char* sql =
        "CREATE TABLE IF NOT EXISTS sync_status ("
        "id INTEGER PRIMARY KEY CHECK(id = 1), "
        "last_sync_time TEXT" 
        ");";

    return sqlite3_exec(db, sql, nullptr, nullptr, nullptr) == SQLITE_OK;
}

bool createAccessLogsTable(sqlite3* db)
{
    const char* sql =
        "CREATE TABLE IF NOT EXISTS access_logs ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "member_id TEXT, "
        "granted INTEGER, "
        "reason TEXT, "
        "timestamp TEXT, "
        "source TEXT, "
        "idempotency_key TEXT UNIQUE" 
        ");";

    if (sqlite3_exec(db, sql, nullptr, nullptr, nullptr) != SQLITE_OK) {
        return false;
    }

    const char* pragmaSql = "PRAGMA table_info(access_logs);";
    sqlite3_stmt* stmt = nullptr;
    bool hasSourceColumn = false;
    bool hasIdempotencyKeyColumn = false;

    if (sqlite3_prepare_v2(db, pragmaSql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char* columnName = sqlite3_column_text(stmt, 1);
        if (columnName) {
            std::string colName = reinterpret_cast<const char*>(columnName);
            if (colName == "source") {
                hasSourceColumn = true;
            } else if (colName == "idempotency_key") {
                hasIdempotencyKeyColumn = true;
            }
        }
    }
    sqlite3_finalize(stmt);

    if (!hasSourceColumn) {
        const char* alterSql = "ALTER TABLE access_logs ADD COLUMN source TEXT;";
        if (sqlite3_exec(db, alterSql, nullptr, nullptr, nullptr) != SQLITE_OK) {
            return false;
        }
    }

    if (!hasIdempotencyKeyColumn) {
        const char* alterSql = "ALTER TABLE access_logs ADD COLUMN idempotency_key TEXT;";
        if (sqlite3_exec(db, alterSql, nullptr, nullptr, nullptr) != SQLITE_OK) {
            return false;
        }
        
        // Create unique index on idempotency_key for existing databases
        const char* indexSql = "CREATE UNIQUE INDEX IF NOT EXISTS idx_access_logs_idempotency_key ON access_logs(idempotency_key);";
        if (sqlite3_exec(db, indexSql, nullptr, nullptr, nullptr) != SQLITE_OK) {
            return false;
        }
    }

    return true;
}

bool initializeSyncStatus(sqlite3* db)
{
    const char* sql = "INSERT OR IGNORE INTO sync_status (id, last_sync_time) VALUES (1, '1970-01-01 00:00:00');";
    return sqlite3_exec(db, sql, nullptr, nullptr, nullptr) == SQLITE_OK;
}

bool createDeviceCommandQueueTable(sqlite3* db)
{
    const char* sql =
        "CREATE TABLE IF NOT EXISTS device_commands ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "device_id TEXT NOT NULL, "
        "command_type TEXT NOT NULL, "
        "payload TEXT, "
        "status TEXT DEFAULT 'PENDING', "
        "created_at TEXT, "
        "updated_at TEXT"
        ");";
    return sqlite3_exec(db, sql, nullptr, nullptr, nullptr) == SQLITE_OK;
}

bool createDeviceStatusTable(sqlite3* db)
{
    const char* sql =
        "CREATE TABLE IF NOT EXISTS device_status ("
        "device_id TEXT PRIMARY KEY, "
        "status TEXT, "
        "last_seen TEXT, "
        "details TEXT, "
        "updated_at TEXT"
        ");";
    return sqlite3_exec(db, sql, nullptr, nullptr, nullptr) == SQLITE_OK;
}

bool queueDeviceCommand(sqlite3* db, const DeviceCommand& command)
{
    const char* sql =
        "INSERT INTO device_commands (device_id, command_type, payload, status, created_at, updated_at) "
        "VALUES (?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    const std::string createdAt = command.created_at.empty() ? getCurrentTimestamp() : command.created_at;
    const std::string updatedAt = command.updated_at.empty() ? createdAt : command.updated_at;

    sqlite3_bind_text(stmt, 1, command.device_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, command.command_type.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, command.payload.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, command.status.empty() ? "PENDING" : command.status.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, createdAt.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, updatedAt.c_str(), -1, SQLITE_TRANSIENT);

    bool success = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return success;
}

std::vector<DeviceCommand> getDeviceCommands(sqlite3* db, const std::string& device_id, bool pending_only)
{
    std::string sql =
        "SELECT id, device_id, command_type, payload, status, created_at, updated_at "
        "FROM device_commands WHERE device_id = ?";
    if (pending_only) {
        sql += " AND status = 'PENDING'";
    }
    sql += " ORDER BY id ASC;";

    sqlite3_stmt* stmt = nullptr;
    std::vector<DeviceCommand> commands;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return commands;
    }

    sqlite3_bind_text(stmt, 1, device_id.c_str(), -1, SQLITE_TRANSIENT);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        DeviceCommand command;
        command.id = sqlite3_column_int(stmt, 0);
        command.device_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        command.command_type = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        command.payload = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        command.status = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        command.created_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        command.updated_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
        commands.push_back(command);
    }
    sqlite3_finalize(stmt);
    return commands;
}

bool markDeviceCommandCompleted(sqlite3* db, int command_id, const std::string& status)
{
    const char* sql = "UPDATE device_commands SET status = ?, updated_at = ? WHERE id = ?;";
    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, status.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, getCurrentTimestamp().c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, command_id);

    bool success = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return success;
}

bool updateDeviceStatus(sqlite3* db, const std::string& device_id, const std::string& status, const std::string& last_seen, const std::string& details)
{
    const char* sql =
        "INSERT OR REPLACE INTO device_status (device_id, status, last_seen, details, updated_at) "
        "VALUES (?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    const std::string updatedAt = getCurrentTimestamp();
    sqlite3_bind_text(stmt, 1, device_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, status.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, last_seen.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, details.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, updatedAt.c_str(), -1, SQLITE_TRANSIENT);

    bool success = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return success;
}

std::optional<DeviceStatus> getDeviceStatus(sqlite3* db, const std::string& device_id)
{
    const char* sql = "SELECT device_id, status, last_seen, details, updated_at FROM device_status WHERE device_id = ?;";
    sqlite3_stmt* stmt = nullptr;
    std::optional<DeviceStatus> result;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return result;
    }

    sqlite3_bind_text(stmt, 1, device_id.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        DeviceStatus status;
        status.device_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        status.status = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        status.last_seen = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        status.details = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        status.updated_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        result = status;
    }

    sqlite3_finalize(stmt);
    return result;
}

std::vector<DeviceStatus> getAllDeviceStatuses(sqlite3* db)
{
    const char* sql = "SELECT device_id, status, last_seen, details, updated_at FROM device_status ORDER BY updated_at DESC;";
    sqlite3_stmt* stmt = nullptr;
    std::vector<DeviceStatus> statuses;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return statuses;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        DeviceStatus status;
        status.device_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        status.status = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        status.last_seen = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        status.details = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        status.updated_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        statuses.push_back(status);
    }

    sqlite3_finalize(stmt);
    return statuses;
}

bool updateLastSyncTime(sqlite3* db, const std::string& timestamp)
{
    const char* sql = "INSERT OR REPLACE INTO sync_status (id, last_sync_time) VALUES (1, ?);";
    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, timestamp.c_str(), -1, SQLITE_TRANSIENT);
    bool success = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return success;
}

bool logAccessAttempt(sqlite3* db, const std::string& member_id, bool granted, const std::string& reason, const std::string& timestamp, const std::string& source, const std::string& idempotency_key)
{
    const char* sql = "INSERT OR IGNORE INTO access_logs (member_id, granted, reason, timestamp, source, idempotency_key) VALUES (?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, member_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, granted ? 1 : 0);
    sqlite3_bind_text(stmt, 3, reason.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, timestamp.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, source.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, idempotency_key.c_str(), -1, SQLITE_TRANSIENT);

    bool success = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return success;
}

static AccessLog readAccessLogRow(sqlite3_stmt* stmt)
{
    AccessLog log;
    log.id = sqlite3_column_int(stmt, 0);
    log.member_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
    log.granted = sqlite3_column_int(stmt, 2) != 0;
    log.reason = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
    log.timestamp = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
    log.source = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
    const unsigned char* idempotencyKeyText = sqlite3_column_text(stmt, 6);
    log.idempotency_key = idempotencyKeyText ? reinterpret_cast<const char*>(idempotencyKeyText) : "";
    return log;
}

std::vector<AccessLog> getAccessLogs(sqlite3* db)
{
    const char* sql = "SELECT id, member_id, granted, reason, timestamp, source, idempotency_key FROM access_logs ORDER BY timestamp DESC;";
    sqlite3_stmt* stmt = nullptr;
    std::vector<AccessLog> logs;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return logs;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        logs.push_back(readAccessLogRow(stmt));
    }
    sqlite3_finalize(stmt);
    return logs;
}

std::vector<AccessLog> getLogsByMember(sqlite3* db, const std::string& member_id)
{
    const char* sql = "SELECT id, member_id, granted, reason, timestamp, source, idempotency_key FROM access_logs WHERE member_id = ? ORDER BY timestamp DESC;";
    sqlite3_stmt* stmt = nullptr;
    std::vector<AccessLog> logs;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return logs;
    }

    sqlite3_bind_text(stmt, 1, member_id.c_str(), -1, SQLITE_TRANSIENT);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        logs.push_back(readAccessLogRow(stmt));
    }
    sqlite3_finalize(stmt);
    return logs;
}

std::vector<AccessLog> getLogsByDate(sqlite3* db, const std::string& date)
{
    const char* sql = "SELECT id, member_id, granted, reason, timestamp, source, idempotency_key FROM access_logs WHERE substr(timestamp, 1, 10) = ? ORDER BY timestamp DESC;";
    sqlite3_stmt* stmt = nullptr;
    std::vector<AccessLog> logs;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return logs;
    }

    sqlite3_bind_text(stmt, 1, date.c_str(), -1, SQLITE_TRANSIENT);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        logs.push_back(readAccessLogRow(stmt));
    }
    sqlite3_finalize(stmt);
    return logs;
}

std::optional<std::string> getLastSyncTime(sqlite3* db)
{
    const char* sql = "SELECT last_sync_time FROM sync_status WHERE id = 1;";
    sqlite3_stmt* stmt = nullptr;
    std::optional<std::string> result;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return result;
    }

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char* text = sqlite3_column_text(stmt, 0);
        if (text) {
            result = reinterpret_cast<const char*>(text);
        }
    }

    sqlite3_finalize(stmt);
    return result;
}

bool insertMember(sqlite3* db, const Member& member)
{
    const char* sql = "INSERT OR IGNORE INTO members (member_id, member_name, member_fingerprint_template, member_status, member_expiring_date, last_updated) VALUES (?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, member.member_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, member.member_name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, member.member_fingerprint_template.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, member.member_status.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, member.member_expiring_date.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, member.last_updated.c_str(), -1, SQLITE_TRANSIENT);

    bool success = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return success;
}

bool updateMember(sqlite3* db, const Member& member)
{
    const char* sql =
        "UPDATE members SET "
        "member_name = ?, "
        "member_fingerprint_template = ?, "
        "member_status = ?, "
        "member_expiring_date = ?, "
        "last_updated = ? "
        "WHERE member_id = ?;";
    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, member.member_name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, member.member_fingerprint_template.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, member.member_status.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, member.member_expiring_date.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, member.last_updated.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, member.member_id.c_str(), -1, SQLITE_TRANSIENT);

    bool success = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return success;
}

bool deleteMember(sqlite3* db, const std::string& member_id)
{
    const char* sql = "DELETE FROM members WHERE member_id = ?;";
    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, member_id.c_str(), -1, SQLITE_TRANSIENT);
    bool success = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return success;
}

bool markMemberInactive(sqlite3* db, const std::string& member_id, const std::string& last_updated)
{
    const char* sql = "UPDATE members SET member_status = 'INACTIVE', last_updated = ? WHERE member_id = ?;";
    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, last_updated.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, member_id.c_str(), -1, SQLITE_TRANSIENT);
    bool success = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return success;
}

std::vector<Member> getAllMembers(sqlite3* db)
{
    const char* sql = "SELECT member_id, member_name, member_fingerprint_template, member_status, member_expiring_date, last_updated FROM members;";
    sqlite3_stmt* stmt = nullptr;
    std::vector<Member> members;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return members;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Member member;
        const unsigned char* idText = sqlite3_column_text(stmt, 0);
        const unsigned char* nameText = sqlite3_column_text(stmt, 1);
        const unsigned char* templateText = sqlite3_column_text(stmt, 2);
        const unsigned char* statusText = sqlite3_column_text(stmt, 3);
        const unsigned char* expiringText = sqlite3_column_text(stmt, 4);
        const unsigned char* updatedText = sqlite3_column_text(stmt, 5);

        member.member_id = idText ? reinterpret_cast<const char*>(idText) : "";
        member.member_name = nameText ? reinterpret_cast<const char*>(nameText) : "";
        member.member_fingerprint_template = templateText ? reinterpret_cast<const char*>(templateText) : "";
        member.member_status = statusText ? reinterpret_cast<const char*>(statusText) : "";
        member.member_expiring_date = expiringText ? reinterpret_cast<const char*>(expiringText) : "";
        member.last_updated = updatedText ? reinterpret_cast<const char*>(updatedText) : "";
        members.push_back(member);
    }

    sqlite3_finalize(stmt);
    return members;
}

std::vector<Member> getMembersChangedSince(sqlite3* db, const std::string& since)
{
    const char* sql = "SELECT member_id, member_name, member_fingerprint_template, member_status, member_expiring_date, last_updated FROM members WHERE last_updated > ?;";
    sqlite3_stmt* stmt = nullptr;
    std::vector<Member> members;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return members;
    }

    sqlite3_bind_text(stmt, 1, since.c_str(), -1, SQLITE_TRANSIENT);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Member member;
        const unsigned char* idText = sqlite3_column_text(stmt, 0);
        const unsigned char* nameText = sqlite3_column_text(stmt, 1);
        const unsigned char* templateText = sqlite3_column_text(stmt, 2);
        const unsigned char* statusText = sqlite3_column_text(stmt, 3);
        const unsigned char* expiringText = sqlite3_column_text(stmt, 4);
        const unsigned char* updatedText = sqlite3_column_text(stmt, 5);

        member.member_id = idText ? reinterpret_cast<const char*>(idText) : "";
        member.member_name = nameText ? reinterpret_cast<const char*>(nameText) : "";
        member.member_fingerprint_template = templateText ? reinterpret_cast<const char*>(templateText) : "";
        member.member_status = statusText ? reinterpret_cast<const char*>(statusText) : "";
        member.member_expiring_date = expiringText ? reinterpret_cast<const char*>(expiringText) : "";
        member.last_updated = updatedText ? reinterpret_cast<const char*>(updatedText) : "";
        members.push_back(member);
    }

    sqlite3_finalize(stmt);
    return members;
}

std::vector<Member> getMembersByStatus(sqlite3* db, const std::string& status)
{
    const char* sql = "SELECT member_id, member_name, member_fingerprint_template, member_status, member_expiring_date, last_updated FROM members WHERE member_status = ?;";
    sqlite3_stmt* stmt = nullptr;
    std::vector<Member> members;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return members;
    }

    sqlite3_bind_text(stmt, 1, status.c_str(), -1, SQLITE_TRANSIENT);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Member member;
        const unsigned char* idText = sqlite3_column_text(stmt, 0);
        const unsigned char* nameText = sqlite3_column_text(stmt, 1);
        const unsigned char* templateText = sqlite3_column_text(stmt, 2);
        const unsigned char* statusText = sqlite3_column_text(stmt, 3);
        const unsigned char* expiringText = sqlite3_column_text(stmt, 4);
        const unsigned char* updatedText = sqlite3_column_text(stmt, 5);

        member.member_id = idText ? reinterpret_cast<const char*>(idText) : "";
        member.member_name = nameText ? reinterpret_cast<const char*>(nameText) : "";
        member.member_fingerprint_template = templateText ? reinterpret_cast<const char*>(templateText) : "";
        member.member_status = statusText ? reinterpret_cast<const char*>(statusText) : "";
        member.member_expiring_date = expiringText ? reinterpret_cast<const char*>(expiringText) : "";
        member.last_updated = updatedText ? reinterpret_cast<const char*>(updatedText) : "";
        members.push_back(member);
    }

    sqlite3_finalize(stmt);
    return members;
}

std::vector<std::string> getAllMemberIds(sqlite3* db)
{
    const char* sql = "SELECT member_id FROM members;";
    sqlite3_stmt* stmt = nullptr;
    std::vector<std::string> ids;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return ids;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char* text = sqlite3_column_text(stmt, 0);
        if (text) {
            ids.push_back(reinterpret_cast<const char*>(text));
        }
    }

    sqlite3_finalize(stmt);
    return ids;
}

bool memberExists(sqlite3* db, const std::string& member_id)
{
    const char* sql = "SELECT COUNT(*) FROM members WHERE member_id = ?";
    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, member_id.c_str(), -1, SQLITE_TRANSIENT);
    bool exists = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        exists = sqlite3_column_int(stmt, 0) > 0;
    }

    sqlite3_finalize(stmt);
    return exists;
}

std::optional<Member> getMemberById(sqlite3* db, const std::string& member_id)
{
    const char* sql = "SELECT member_id, member_name, member_fingerprint_template, member_status, member_expiring_date, last_updated FROM members WHERE member_id = ?;";
    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return std::nullopt;
    }

    sqlite3_bind_text(stmt, 1, member_id.c_str(), -1, SQLITE_TRANSIENT);

    std::optional<Member> result;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        Member member;
        const unsigned char* idText = sqlite3_column_text(stmt, 0);
        const unsigned char* nameText = sqlite3_column_text(stmt, 1);
        const unsigned char* templateText = sqlite3_column_text(stmt, 2);
        const unsigned char* statusText = sqlite3_column_text(stmt, 3);
        const unsigned char* expiringText = sqlite3_column_text(stmt, 4);
        const unsigned char* updatedText = sqlite3_column_text(stmt, 5);

        member.member_id = idText ? reinterpret_cast<const char*>(idText) : "";
        member.member_name = nameText ? reinterpret_cast<const char*>(nameText) : "";
        member.member_fingerprint_template = templateText ? reinterpret_cast<const char*>(templateText) : "";
        member.member_status = statusText ? reinterpret_cast<const char*>(statusText) : "";
        member.member_expiring_date = expiringText ? reinterpret_cast<const char*>(expiringText) : "";
        member.last_updated = updatedText ? reinterpret_cast<const char*>(updatedText) : "";
        result = member;
    }

    sqlite3_finalize(stmt);
    return result;
}

void closeDatabase(sqlite3* db)
{
    if (db) {
        sqlite3_close(db);
    }
}
