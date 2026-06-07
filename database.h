#pragma once

#include <string>
#include <vector>
#include <sqlite3.h>
#include "member.h"

bool openDatabase(sqlite3*& db, const std::string& path);
bool createMembersTable(sqlite3* db);
bool insertMember(sqlite3* db, const Member& member);
bool updateMember(sqlite3* db, const Member& member);
bool deleteMember(sqlite3* db, const std::string& member_id);
bool memberExists(sqlite3* db, const std::string& member_id);
std::vector<std::string> getAllMemberIds(sqlite3* db);
void closeDatabase(sqlite3* db);
