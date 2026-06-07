#pragma once

class RelayInterface {
public:
    virtual ~RelayInterface() = default;
    virtual bool turnOn() = 0;
    virtual bool turnOff() = 0;
};
