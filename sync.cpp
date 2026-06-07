#include "sync.h"
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

static void logError(const std::string& message)
{
    std::ofstream logFile("sync.log", std::ios::app);
    if (logFile.is_open()) {
        logFile << getCurrentTimestamp() << " - " << message << std::endl;
    }
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

void removeMissingMembers(sqlite3* db, const std::vector<Member>& cloudMembers)
{
    std::set<std::string> cloudIds;
    for (const auto& m : cloudMembers) {
        cloudIds.insert(m.member_id);
    }

    for (const auto& localId : getAllMemberIds(db)) {
        if (cloudIds.find(localId) == cloudIds.end()) {
            if (deleteMember(db, localId)) {
                logError("Deleted member: " + localId);
            }
        }
    }
}

void synchronizeDeletedMembers(sqlite3* db, const std::vector<Member>& sourceMembers, bool hardDelete)
{
    if (hardDelete) {
        removeMissingMembers(db, sourceMembers);
    }
}
