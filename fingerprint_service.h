#pragma once

#include <string>
#include <sqlite3.h>
#include "api_client.h"
#include "fingerprint_scanner_interface.h"

class FingerprintService {
public:
    FingerprintService(ApiClient& apiClient, sqlite3* db, FingerprintScannerInterface* scanner);

    bool startEnrollment(const std::string& memberId);
    bool verifyFingerprint();

private:
    ApiClient& apiClient_;
    sqlite3* db_;
    FingerprintScannerInterface* scanner_;
    std::string pendingMemberId_;
};
