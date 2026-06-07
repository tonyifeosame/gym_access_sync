#pragma once

#include <string>

class FingerprintScannerInterface {
public:
    virtual ~FingerprintScannerInterface() = default;
    virtual bool promptForFinger() = 0;
    virtual std::string captureFingerprintTemplate() = 0;
    virtual bool startEnrollment(const std::string& memberId) = 0;
    virtual bool verifyFingerprint(std::string& memberId) = 0;
};
