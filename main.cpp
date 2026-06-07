#include <iostream>
#include <vector>
#include <string>
#include <set>
#include <fstream>
#include <sqlite3.h>
using namespace std;
struct Member{
    string member_id;
    string member_name;
    string member_fingerprint_template;
    string member_status;
    string member_expiring_date;
    string last_updated;
    
};

struct ValidationResult{
    bool is_valid;
    vector<string> errors;
};
ValidationResult validateFingerprint(const Member& member)
{
    ValidationResult result{true, {}};
    if (member.member_fingerprint_template.empty()) {
        result.is_valid = false;
        result.errors.push_back("Fingerprint template is empty");
    }
    return result;
}

ValidationResult validateName(const Member& member)
{
    ValidationResult result{true, {}};
    if (member.member_name.empty()) {
        result.is_valid = false;
        result.errors.push_back("Name is empty");
    }
    return result;
}
ValidationResult validateStatus(const Member& member)
{
    ValidationResult result{true, {}};
    if (member.member_status != "ACTIVE" && member.member_status != "INACTIVE") {
        result.is_valid = false;
        result.errors.push_back("Invalid status");
    }
    return result;
}
ValidationResult validateExpiringDate(const Member& member)
{
    ValidationResult result{true, {}};
    if (member.member_expiring_date.empty()) {
        result.is_valid = false;
        result.errors.push_back("Expiring date is empty");
    }
    return result;
}
ValidationResult validateMemberId(const Member& member)
{
    ValidationResult result{true, {}};
    if (member.member_id.empty()) {
        result.is_valid = false;
        result.errors.push_back("Member ID is empty");
    }
    return result;
}
bool validateDuplicateIds(const vector<Member>& members)
{
    for (size_t i = 0; i < members.size(); ++i) {
        for (size_t j = i + 1; j < members.size(); ++j) {
            if (members[i].member_id == members[j].member_id) {
                return false;
            }
        }
    }
    return true;
}
set<string> getDuplicateIds(const vector<Member>& members)
{
    set<string> ids;
    set<string> duplicateIds;
    for (const auto& member : members) {
        if (ids.find(member.member_id) != ids.end()) {
            duplicateIds.insert(member.member_id);
        } else {
            ids.insert(member.member_id);
        }
    }
    return duplicateIds;
}
bool validateMember(const Member& member)
{
    return validateFingerprint(member).is_valid &&
           validateName(member).is_valid &&
           validateStatus(member).is_valid &&
           validateExpiringDate(member).is_valid &&
           validateMemberId(member).is_valid;
}
void logError(const string& message)
{
    ofstream logFile("sync.log", ios::app);

    if (logFile.is_open())
    {
        logFile << message << endl;
        logFile.close();
    }
}
bool memberExists(sqlite3* db, const string& member_id)
{
    sqlite3_stmt* stmt;

    const char* sql =
        "SELECT COUNT(*) FROM members WHERE member_id = ?";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        return false;
    }

    sqlite3_bind_text(
        stmt,
        1,
        member_id.c_str(),
        -1,
        SQLITE_TRANSIENT
    );

    bool exists = false;

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        exists = sqlite3_column_int(stmt, 0) > 0;
    }

    sqlite3_finalize(stmt);

    return exists;
}

bool insertMember(sqlite3* db, const Member& member)
{
    const char* sql = "INSERT OR IGNORE INTO members (member_id, member_name, member_fingerprint_template, member_status, member_expiring_date, last_updated ) VALUES (?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        cout << sqlite3_errmsg(db) << endl;
        return false;
    }

    sqlite3_bind_text(stmt, 1, member.member_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, member.member_name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, member.member_fingerprint_template.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, member.member_status.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, member.member_expiring_date.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, member.last_updated.c_str(), -1, SQLITE_TRANSIENT);
    

    int rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        cout << "SQLite Error: " << sqlite3_errmsg(db) << endl;
    }

    bool success = (rc == SQLITE_DONE);
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

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        return false;
    }

    sqlite3_bind_text(stmt, 1, member.member_name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, member.member_fingerprint_template.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, member.member_status.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, member.member_expiring_date.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, member.last_updated.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, member.member_id.c_str(), -1, SQLITE_TRANSIENT);

    bool success = (sqlite3_step(stmt) == SQLITE_DONE);

    sqlite3_finalize(stmt);

    return success;
}

int main() {
     vector<Member> members;
   vector<Member> changedMembers;
   
    Member member1 = {
        "001",
        "John Doe",
        "template1",
        "ACTIVE",
        "2024-12-31",
        "2026-06-06 10:00:00",
        
    };
   
    Member member2 = {
        "002",
        "Jane Smith",
        "template2",
        "INACTIVE",
        "2023-06-30",
        "2026-06-06 10:05:00",
        
    };
    
    Member member3 = {
        "003",
        "Alice Johnson",
        "",
        "ACTIVE",
        "2025-03-15",
        "2026-06-06 10:10:00",
        
    };
    
    Member member4 = {
        "004",
        "",
        "template4",
        "ACTIVE",
        "2024-09-30",
        "2026-06-06 10:15:00",
        
    };
    members.push_back(member1);
members.push_back(member2);
members.push_back(member3);
members.push_back(member4);
    sqlite3* db;

if (sqlite3_open("members.db", &db) != SQLITE_OK)
{
    cout << "Failed to open database" << endl;
    return 1;
}

    const char* createTableSql =
    "CREATE TABLE IF NOT EXISTS members ("
    "member_id TEXT PRIMARY KEY, "
    "member_name TEXT, "
    "member_fingerprint_template TEXT, "
    "member_status TEXT, "
    "member_expiring_date TEXT, "
    "last_updated TEXT"
    ");";
    sqlite3_exec(db, createTableSql, nullptr, nullptr, nullptr);

   
   for (const auto& member : members)
{
    if (memberExists(db, member.member_id))
    {
        cout << "Updating member: "
             << member.member_id
             << endl;

        updateMember(db, member);
    }
    else
    {
        cout << "Inserting member: "
             << member.member_id
             << endl;

        insertMember(db, member);
    }
}

    if (!validateDuplicateIds(members)) {
        cout << "Duplicate IDs found. Synchronization aborted." << endl;
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

    cout << "Valid Members: " << validCount << endl;
    cout << "Invalid Members: " << invalidCount << endl;
 

cout << "001 exists: "
     << memberExists(db, "001")
     << endl;

cout << "999 exists: "
     << memberExists(db, "999")
     << endl;

     Member updatedMember = {
    "001",
    "John Doe",
    "template1",
    "INACTIVE",
    "2024-12-31",
    "2026-06-07 09:00:00"
};
cout << "Update member1: "
     << updateMember(db, updatedMember)
     << endl;
    logError("Program started");
    for (const auto& member : members) {
        if (!validateMember(member)) {
            logError("Invalid member: " + member.member_id);
        }
    }
    logError("Synchronization completed. Valid Members: " + to_string(validCount) + ", Invalid Members: " + to_string(invalidCount));
sqlite3_close(db);
    return 0;
}
