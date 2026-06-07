#pragma once

#include <string>
#include <vector>
#include "member.h"

struct ValidationResult {
    bool is_valid;
    std::vector<std::string> errors;
};

ValidationResult validateFingerprint(const Member& member);
ValidationResult validateName(const Member& member);
ValidationResult validateStatus(const Member& member);
ValidationResult validateExpiringDate(const Member& member);
ValidationResult validateMemberId(const Member& member);
bool validateMember(const Member& member);
bool validateDuplicateIds(const std::vector<Member>& members);
std::vector<std::string> getDuplicateIds(const std::vector<Member>& members);
