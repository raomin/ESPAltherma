#pragma once

#include <Arduino.h>
#include "command.hpp"

enum class CAN_ICTypes : uint8_t {
    None,
    MCP2515,
    ELM327,
    SJA1000
};

enum class CAN_ICBus : uint8_t {
    None,
    SPI,
    UART,
    BT
};

enum class CAN_PollMode : uint8_t {
    Disabled,
    Passiv,
    Auto
};

struct DriverBluetoothConfig {
    String DEVICENAME;
    bool USE_PIN;
    String PIN;
};

struct DriverSPIConfig {
    uint8_t PIN_MOSI;
    uint8_t PIN_MISO;
    uint8_t PIN_SCK;
    uint8_t PIN_CS;
    uint8_t PIN_INT;
    uint8_t IC_MHZ;
};

struct DriverUARTConfig {
    uint8_t PIN_RX;
    uint8_t PIN_TX;
};

struct CAN_Config
{
    CAN_ICTypes CAN_IC;
    CAN_ICBus CAN_BUS;
    DriverUARTConfig CAN_UART;
    DriverSPIConfig CAN_SPI;
    DriverBluetoothConfig CAN_BLUETOOTH;
    uint16_t CAN_SPEED_KBPS;
    String CAN_MQTT_TOPIC_NAME;
    bool CAN_READONLY_ENABLED;
    bool CAN_SNIFFING_ENABLED;
    CAN_PollMode CAN_AUTOPOLL_MODE;
    uint16_t CAN_AUTOPOLL_TIME;
    size_t COMMANDS_LENGTH;
    CANCommand** COMMANDS;

    ~CAN_Config();
};