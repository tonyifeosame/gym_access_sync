#include "sync.h"
#include "api_client.h"
#include "database.h"
#include "validation.h"

#include <chrono>
#include <fstream>
#include <iomanip>
#include <set>
#include <sstream>

static std::string getCurrentTimestamp()
{
    auto now = std::chrono::system_clock::now();
    std::time_t tt = std::chrono::system_clock::to_time_t(now);
    std::tm local_tm;
#if defined(_WIN32) || defined(_WIN64)
    localtime_s(&local_tm, &tt);
#else
    localtime_r(&tt, &local_tm);
#endif
    std::ostringstream oss;
    oss << std::put_time(&local_tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

static void writeLog(const std::string& level, const std::string& message)
{
    std::ofstream logFile("sync.log", std::ios::app);
    if (logFile.is_open()) {
        logFile << getCurrentTimestamp() << " [" << level << "] " << message << std::endl;
    }
}

void logInfo(const std::string& message)
{
    writeLog("INFO", message);
}

void logError(const std::string& message)
{
    writeLog("ERROR", message);
}

bool recordLastSync(sqlite3* db)
{
    return updateLastSyncTime(db, getCurrentTimestamp());
}

static void processMember(sqlite3* db, const Member& member)
{
    if (!validateMember(member)) {
        logError("Invalid member: " + member.member_id);
        return;
    }

    if (memberExists(db, member.member_id)) {
        if (updateMember(db, member)) {
            logError("Updated member: " + member.member_id);
        }
    } else {
        if (insertMember(db, member)) {
            logError("Inserted member: " + member.member_id);
        }
    }
}

void synchronizeMembers(sqlite3* db, const std::vector<Member>& members)
{
    for (const auto& member : members) {
        processMember(db, member);
    }
}

int removeMissingMembers(sqlite3* db, const std::vector<Member>& cloudMembers, bool dryRun, bool useSoftDelete)
{
    std::set<std::string> cloudIds;
    for (const auto& m : cloudMembers) {
        cloudIds.insert(m.member_id);
    }

    std::vector<std::string> localIds = getAllMemberIds(db);
    int candidates = 0;
    int deleted = 0;

    char* errmsg = nullptr;
    if (!dryRun) {
        if (sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, &errmsg) != SQLITE_OK) {
            std::string em = errmsg ? errmsg : "unknown";
            sqlite3_free(errmsg);
            logError("Failed to begin transaction: " + em);
            return -1;
        }
    }

    for (const auto& localId : localIds) {
        if (cloudIds.find(localId) == cloudIds.end()) {
            candidates++;
            if (dryRun) {
                logError("Dry-run delete candidate: " + localId);
            } else {
                bool success = useSoftDelete
                    ? markMemberInactive(db, localId, getCurrentTimestamp())
                    : deleteMember(db, localId);

                if (success) {
                    deleted++;
                    if (useSoftDelete) {
                        logError("Inactivated member: " + localId);
                    } else {
                        logError("Deleted member: " + localId);
                    }
                } else {
                    logError("Failed to " + std::string(useSoftDelete ? "inactivate" : "delete") + " member: " + localId);
                    sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
                    return -1;
                }
            }
        }
    }

    if (!dryRun) {
        if (sqlite3_exec(db, "COMMIT;", nullptr, nullptr, &errmsg) != SQLITE_OK) {
            std::string em = errmsg ? errmsg : "unknown";
            sqlite3_free(errmsg);
            sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
            logError("Failed to commit transaction: " + em);
            return -1;
        }
        if (errmsg) sqlite3_free(errmsg);
    }

    return dryRun ? candidates : deleted;
}

void synchronizeDeletedMembers(sqlite3* db, const std::vector<Member>& sourceMembers, bool hardDelete)
{
    int res = removeMissingMembers(db, sourceMembers, false, !hardDelete);
    if (res >= 0) {
        logInfo(std::string("removeMissingMembers affected count: ") + std::to_string(res));
    } else {
        logError("removeMissingMembers encountered an error");
    }
}

bool runSyncCycle(sqlite3* db, ApiClient& apiClient)
{
    auto lastSync = getLastSyncTime(db).value_or("1970-01-01 00:00:00");
    logInfo("Starting sync cycle. Last successful sync: " + lastSync);

    if (lastSync == "1970-01-01 00:00:00") {
        auto members = apiClient.fetchAllMembers(db);
        if (!validateDuplicateIds(members)) {
            logError("Duplicate IDs found in cloud member list. Synchronization aborted.");
            return false;
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

        logInfo("Full sync: " + std::to_string(members.size()) + " members (" +
                std::to_string(validCount) + " valid, " + std::to_string(invalidCount) + " skipped)");
        synchronizeMembers(db, members);
        synchronizeDeletedMembers(db, members);
    } else {
        auto changedMembers = apiClient.fetchChangedMembers(lastSync, db);
        logInfo("Incremental sync: " + std::to_string(changedMembers.size()) + " changed members");

        if (!changedMembers.empty()) {
            if (!validateDuplicateIds(changedMembers)) {
                logError("Duplicate IDs found in changed member list. Synchronization aborted.");
                return false;
            }
            synchronizeMembers(db, changedMembers);
        }
        logInfo("Skipping missing-member cleanup for incremental sync.");
    }

    if (!recordLastSync(db)) {
        logError("Failed to record sync timestamp");
        return false;
    }

    auto newLastSync = getLastSyncTime(db).value_or("unknown");
    logInfo("Sync cycle completed. Last successful sync: " + newLastSync);
    return true;
}
