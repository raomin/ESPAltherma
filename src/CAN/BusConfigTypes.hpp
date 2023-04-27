#ifndef BUS_CONFIG_TYPES_H
#define BUS_CONFIG_TYPES_H

#include <Arduino.h>

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

#endif