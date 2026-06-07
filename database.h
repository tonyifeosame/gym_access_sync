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
std::vector<std::string> getAllMemberIds(sqlite3* db);
void closeDatabase(sqlite3* db);
