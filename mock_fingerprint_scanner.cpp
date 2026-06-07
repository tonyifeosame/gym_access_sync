#include "mock_fingerprint_scanner.h"
#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>

bool MockFingerprintScanner::promptForFinger()
{
    std::cout << "MockFingerprintScanner: Please place your finger on the scanner..." << std::endl;
    return true;
}

std::string MockFingerprintScanner::captureFingerprintTemplate()
{
    auto now = std::chrono::system_clock::now();
    auto tt = std::chrono::system_clock::to_time_t(now);
    std::tm local_tm;
#if defined(_WIN32) || defined(_WIN64)
    localtime_s(&local_tm, &tt);
#else
    localtime_r(&tt, &local_tm);
#endif
    std::ostringstream oss;
    oss << "mock-fingerprint-" << std::put_time(&local_tm, "%Y%m%d%H%M%S");
    std::cout << "MockFingerprintScanner: Captured fingerprint template: " << oss.str() << std::endl;
    return oss.str();
}

bool MockFingerprintScanner::startEnrollment(const std::string& memberId)
{
    std::cout << "MockFingerprintScanner: startEnrollment for member " << memberId << std::endl;
    pendingMemberId_ = memberId;
    return true;
}

bool MockFingerprintScanner::verifyFingerprint(std::string& memberId)
{
    if (pendingMemberId_.empty()) {
        std::cerr << "MockFingerprintScanner: no pending enrollment" << std::endl;
        return false;
    }

    std::cout << "MockFingerprintScanner: verifyFingerprint for member " << pendingMemberId_ << std::endl;
    memberId = pendingMemberId_;
    return true;
}
