#include "door_controller.h"
#include <iostream>
#include <thread>
#include <chrono>

DoorController::DoorController(RelayInterface* relay)
    : relay_(relay)
{
}

bool DoorController::unlock(int seconds)
{
    if (!relay_) {
        std::cerr << "DoorController error: relay interface not provided" << std::endl;
        return false;
    }

    if (!relay_->turnOn()) {
        std::cerr << "DoorController error: failed to turn relay on" << std::endl;
        return false;
    }

    std::cout << "Door unlocked for " << seconds << " seconds" << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(seconds));

    if (!relay_->turnOff()) {
        std::cerr << "DoorController error: failed to turn relay off" << std::endl;
        return false;
    }

    std::cout << "Door locked" << std::endl;
    return true;
}
