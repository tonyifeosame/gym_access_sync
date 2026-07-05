#pragma once

#include <string>

class IFingerprintDevice {
public:
    virtual ~IFingerprintDevice() = default;

    virtual bool simulateEnrollment(const std::string& member_id) = 0;
    virtual bool simulateVerification(const std::string& member_id) = 0;
    virtual bool simulateUnlock(int seconds) = 0;
    virtual bool simulateHeartbeat() = 0;
    virtual const std::string& getDeviceId() const = 0;
    virtual const std::string& getLastEvent() const = 0;
};
