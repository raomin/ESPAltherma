#ifndef DRIVER_MCP2515_H
#define DRIVER_MCP2515_H

#include <SPI.h>
#include <107-Arduino-MCP2515.h>
#include "CAN/Driver.hpp"
#include "debugSerial.hpp"
#include "MQTT/mqtt.hpp"
#include "Config/config.hpp"

using namespace MCP2515;

class DriverMCP2515 : public CANDriver
{
private:
  ArduinoMCP2515* mcp2515;
  const SPISettings MCP2515x_SPI_SETTING{1000000, MSBFIRST, SPI_MODE0};
  bool setMode(CanDriverMode mode);
  bool getRate(const uint8_t mhz, const uint16_t speed, CanBitRate &rate);

public:
  DriverMCP2515(const CAN_Config* CANConfig);
  bool initInterface();
  void writeLoopbackTest();
  void sendCommand(CANCommand* cmd, bool setValue = false, int value = 0);
  void handleInterrupt();
  ~DriverMCP2515();
};

#endif