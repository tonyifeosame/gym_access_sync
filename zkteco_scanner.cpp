#include "zkteco_scanner.h"
#include <iostream>

ZKTecoScanner::ZKTecoScanner(const std::string& ip, int port, int timeout)
    : ip_(ip)
    , port_(port)
    , timeout_(timeout)
{
}

bool ZKTecoScanner::promptForFinger()
{
    std::cout << "[ZKTeco] promptForFinger placeholder" << std::endl;
    return true;
}

std::string ZKTecoScanner::captureFingerprintTemplate()
{
    std::cout << "[ZKTeco] captureFingerprintTemplate placeholder" << std::endl;
    return "";
}

bool ZKTecoScanner::startEnrollment(const std::string& memberId)
{
    std::cout << "[ZKTeco] IP: " << ip_
              << " Port: " << port_
              << " Enroll: " << memberId
              << std::endl;
    pendingMemberId_ = memberId;
    return true;
}

bool ZKTecoScanner::verifyFingerprint(std::string& memberId)
{
    if (pendingMemberId_.empty()) {
        std::cerr << "[ZKTeco] no pending enrollment" << std::endl;
        return false;
    }

    memberId = pendingMemberId_;
    std::cout << "[ZKTeco] verifyFingerprint for member " << memberId << " (placeholder)" << std::endl;
    pendingMemberId_.clear();
    return true;
}

bool ZKTecoScanner::connect()
{
    std::cout << "[ZKTeco] connect() to " << ip_ << ":" << port_ << " (placeholder)" << std::endl;
    return true;
}
