#include "mock_esp32_device.h"

#include <iostream>
#include <utility>

MockEsp32Device::MockEsp32Device(std::string device_id) : device_id_(std::move(device_id)) {}

bool MockEsp32Device::simulateEnrollment(const std::string& member_id)
{
    last_event_ = "ENROLL:" + member_id;
    events_.push_back(last_event_);
    std::cout << "MockEsp32Device: enrolled member " << member_id << std::endl;
    return true;
}

bool MockEsp32Device::simulateVerification(const std::string& member_id)
{
    last_event_ = "VERIFY:" + member_id;
    events_.push_back(last_event_);
    std::cout << "MockEsp32Device: verified member " << member_id << std::endl;
    return true;
}

bool MockEsp32Device::simulateUnlock(int seconds)
{
    last_event_ = "UNLOCK:" + std::to_string(seconds);
    events_.push_back(last_event_);
    std::cout << "MockEsp32Device: unlock requested for " << seconds << " seconds" << std::endl;
    return true;
}

bool MockEsp32Device::simulateHeartbeat()
{
    last_event_ = "HEARTBEAT:" + device_id_;
    events_.push_back(last_event_);
    std::cout << "MockEsp32Device: heartbeat sent" << std::endl;
    return true;
}

const std::string& MockEsp32Device::getDeviceId() const
{
    return device_id_;
}

const std::string& MockEsp32Device::getLastEvent() const
{
    return last_event_;
}
