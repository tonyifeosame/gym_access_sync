#include "database.h"
#include <iostream>

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
