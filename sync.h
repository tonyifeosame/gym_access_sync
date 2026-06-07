#pragma once

#include <vector>
#include <sqlite3.h>
#include "member.h"

void synchronizeMembers(sqlite3* db, const std::vector<Member>& members);
void synchronizeDeletedMembers(sqlite3* db, const std::vector<Member>& sourceMembers, bool hardDelete = true);
// Removes local members not present in cloudMembers.
// Returns number of deleted members, or -1 on error. If dryRun is true,
// no deletions are performed and the function returns the number of
// candidates that would be deleted.
int removeMissingMembers(sqlite3* db, const std::vector<Member>& cloudMembers, bool dryRun = false);
