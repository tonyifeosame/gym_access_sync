#pragma once

#include "fingerprint_device_interface.h"

#include <string>
#include <vector>

class MockEsp32Device : public IFingerprintDevice {
public:
    explicit MockEsp32Device(std::string device_id);

    bool simulateEnrollment(const std::string& member_id) override;
    bool simulateVerification(const std::string& member_id) override;
    bool simulateUnlock(int seconds) override;
    bool simulateHeartbeat() override;

    const std::string& getDeviceId() const override;
    const std::string& getLastEvent() const override;

private:
    std::string device_id_;
    std::string last_event_;
    std::vector<std::string> events_;
};
