#include <iostream>
#include <vector>
#include <string>
#include <set>
using namespace std;
struct Member{
    string member_id;
    string member_name;
    string member_fingerprint_template;
    string member_status;
    string member_expiring_date;
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

int main(){
    vector<Member> members;
    Member member1 = {
        "001",
        "John Doe",
        "template1",
        "ACTIVE",
        "2024-12-31"
    };
    members.push_back(member1);
    Member member2 = {
        "002",
        "Jane Smith",
        "template2",
        "INACTIVE",
        "2023-06-30"
    };
    members.push_back(member2);
    Member member3 = {
        "003",
        "Alice Johnson",
        "",
        "ACTIVE",
        "2025-03-15"
    };
    members.push_back(member3);
    Member member4 = {
        "004",
        "",
        "template4",
        "ACTIVE",
        "2024-09-30"
    };
    members.push_back(member4);
    
    int validCount = 0;
    if (!validateDuplicateIds(members))
{
    cout << "Duplicate IDs found. Synchronization aborted." << endl;
    return 1;
}
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
    set<string> ids;
    for (const auto& member : members) {
        if (ids.find(member.member_id) != ids.end()) {
            cout << "Duplicate ID found: " << member.member_id << endl;
        } else {
            ids.insert(member.member_id);
        }
    }
    return 0;
}
