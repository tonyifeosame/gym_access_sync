#pragma once

#include <vector>
#include <sqlite3.h>
#include "member.h"

void synchronizeMembers(sqlite3* db, const std::vector<Member>& members);
void synchronizeDeletedMembers(sqlite3* db, const std::vector<Member>& sourceMembers, bool hardDelete = true);
void removeMissingMembers(sqlite3* db, const std::vector<Member>& cloudMembers);
