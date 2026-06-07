#pragma once

#include <string>
#include <vector>
#include <sqlite3.h>
#include "member.h"

void logError(const std::string& message);
void synchronizeMembers(sqlite3* db, const std::vector<Member>& members);
void synchronizeDeletedMembers(sqlite3* db, const std::vector<Member>& sourceMembers, bool hardDelete = true);
bool recordLastSync(sqlite3* db);
// Removes local members not present in cloudMembers.
// Returns number of deleted or inactivated members, or -1 on error.
// If dryRun is true, no changes are made and the function returns
// the number of candidates that would be affected.
int removeMissingMembers(sqlite3* db, const std::vector<Member>& cloudMembers, bool dryRun = false, bool useSoftDelete = false);
