#pragma once

#include "relay_interface.h"

class DoorController {
public:
    explicit DoorController(RelayInterface* relay);
    bool unlock(int seconds);

private:
    RelayInterface* relay_;
};
