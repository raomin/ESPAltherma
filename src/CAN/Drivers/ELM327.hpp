#ifndef DRIVER_ELM327_H
#define DRIVER_ELM327_H

#include "ArduinoC.hpp"
#include "CAN/CANDriver.hpp"
#include "Config/config.hpp"
#include "debugSerial.hpp"
#include "BluetoothSerial.h"

class DriverELM327 : public CANDriver
{
private:
    bool write(const char *bytes, size_t size);
    bool setID(const uint16_t id);
    bool setMode(CanDriverMode mode);
    bool ATCommandIsOK();
    bool listenOnly;
    String received = "";
    Stream* Elm327Serial;

public:
    bool initInterface();
    void handleLoop();
    void sendCommand(CommandDef* cmd, bool setValue = false, int value = 0);
};

#endif