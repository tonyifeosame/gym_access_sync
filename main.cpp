#include <iostream>
#include <vector>
#include <optional>
#include "member.h"
#include "validation.h"
#include "database.h"
#include "sync.h"

bool hasAccess(const Member& member)
{
    return member.member_status == "ACTIVE";
}

void simulateScan(sqlite3* db, const std::string& member_id)
{
    std::cout << "Simulating scan for member " << member_id << std::endl;
    auto memberOpt = getMemberById(db, member_id);
    if (!memberOpt) {
        std::cout << "Member not found: " << member_id << std::endl;
        return;
    }

    if (hasAccess(*memberOpt)) {
        std::cout << "Access Granted" << std::endl;
        logError("Access Granted: " + member_id);
    } else {
        std::cout << "Access Denied" << std::endl;
        logError("Access Denied: " + member_id);
    }
}

int main()
{
    sqlite3* db = nullptr;
    if (!openDatabase(db, "members.db")) {
        std::cout << "Failed to open database" << std::endl;
        return 1;
    }

    if (!createMembersTable(db)) {
        std::cout << "Failed to create members table" << std::endl;
        closeDatabase(db);
        return 1;
    }

    std::vector<Member> members = {
        {"002", "Jane Smith", "template2", "INACTIVE", "2023-06-30", "2026-06-06 10:05:00"},
        {"003", "Alice Johnson", "", "ACTIVE", "2025-03-15", "2026-06-06 10:10:00"},
        {"004", "", "template4", "ACTIVE", "2024-09-30", "2026-06-06 10:15:00"}
    };

    std::vector<Member> changedMembers = {
        {"001", "John Doe", "template1", "INACTIVE", "2024-12-31", "2026-06-08 09:00:00"},
        {"005", "New Member", "template5", "ACTIVE", "2025-12-31", "2026-06-08 08:00:00"}
    };

    if (!validateDuplicateIds(members)) {
        std::cout << "Duplicate IDs found. Synchronization aborted." << std::endl;
        closeDatabase(db);
        return 1;
    }

    if (!validateDuplicateIds(changedMembers)) {
        std::cout << "Duplicate IDs found in changed members." << std::endl;
        closeDatabase(db);
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

    std::cout << "Members size: " << members.size() << std::endl;
    std::cout << "Valid Members: " << validCount << std::endl;
    std::cout << "Invalid Members: " << invalidCount << std::endl;
    std::cout << "Starting synchronization..." << std::endl;
    

    std::vector<Member> sourceMembers = members;
    sourceMembers.insert(sourceMembers.end(), changedMembers.begin(), changedMembers.end());

    synchronizeMembers(db, sourceMembers);
    synchronizeDeletedMembers(db, sourceMembers);
    
    std::cout << "Synchronization completed." << std::endl;

    simulateScan(db, "001");
    simulateScan(db, "002");
    simulateScan(db, "005");

    closeDatabase(db);
    return 0;
}
