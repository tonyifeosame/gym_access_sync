#include "mock_relay.h"
#include <iostream>

bool MockRelay::turnOn()
{
    std::cout << "MockRelay: ON" << std::endl;
    return true;
}

bool MockRelay::turnOff()
{
    std::cout << "MockRelay: OFF" << std::endl;
    return true;
}
