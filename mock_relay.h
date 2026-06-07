#pragma once

#include "relay_interface.h"

class MockRelay : public RelayInterface {
public:
    bool turnOn() override;
    bool turnOff() override;
};
