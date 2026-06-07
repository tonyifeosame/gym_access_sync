#include "validation.h"

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

bool validateMember(const Member& member)
{
    return validateFingerprint(member).is_valid &&
           validateName(member).is_valid &&
           validateStatus(member).is_valid &&
           validateExpiringDate(member).is_valid &&
           validateMemberId(member).is_valid;
}

bool validateDuplicateIds(const std::vector<Member>& members)
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

std::vector<std::string> getDuplicateIds(const std::vector<Member>& members)
{
    std::vector<std::string> duplicateIds;
    for (size_t i = 0; i < members.size(); ++i) {
        for (size_t j = i + 1; j < members.size(); ++j) {
            if (members[i].member_id == members[j].member_id) {
                duplicateIds.push_back(members[i].member_id);
            }
        }
    }
    return duplicateIds;
}
