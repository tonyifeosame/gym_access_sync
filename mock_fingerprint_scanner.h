#pragma once

#include "fingerprint_scanner_interface.h"

class MockFingerprintScanner : public FingerprintScannerInterface {
public:
    bool promptForFinger() override;
    std::string captureFingerprintTemplate() override;
    bool startEnrollment(const std::string& memberId) override;
    bool verifyFingerprint(std::string& memberId) override;

private:
    std::string pendingMemberId_;
};
