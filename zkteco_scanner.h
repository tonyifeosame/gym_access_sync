#pragma once

#include "fingerprint_scanner_interface.h"
#include <string>

class ZKTecoScanner : public FingerprintScannerInterface {
public:
    ZKTecoScanner(const std::string& ip, int port, int timeout);
    ~ZKTecoScanner() override = default;

    bool promptForFinger() override;
    std::string captureFingerprintTemplate() override;

    bool startEnrollment(const std::string& memberId) override;
    bool verifyFingerprint(std::string& memberId) override;
    bool connect();

private:
    std::string ip_;
    int port_;
    int timeout_;
    std::string pendingMemberId_;
};
