#pragma once

#include <optional>
#include <string>
#include <vector>
#include "member.h"

struct MockHttpRequest {
    std::string method;
    std::string path;
    std::string query;
    std::string apiKey;
    std::string body;
};

struct MockHttpResponse {
    int status;
    std::string body;
};

struct AccessInfo {
    std::string member_id;
    std::string status;
    bool access;
};

MockHttpResponse mockApiServer(const MockHttpRequest& request, const std::vector<Member>& cloudMembers);
std::vector<Member> mockApiClientGetAllMembers(const std::vector<Member>& cloudMembers);
std::vector<Member> mockApiClientGetChanges(const std::string& since, const std::vector<Member>& cloudMembers);
std::optional<Member> mockApiClientGetMember(const std::string& member_id, const std::vector<Member>& cloudMembers);
std::optional<AccessInfo> mockApiClientGetAccess(const std::string& member_id, const std::vector<Member>& cloudMembers);
