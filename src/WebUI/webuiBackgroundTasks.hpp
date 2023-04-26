#ifndef WEBUI_BACKGROUND_TASKS_H
#define WEBUI_BACKGROUND_TASKS_H

#include "ArduinoC.hpp"
#include "X10A/X10A.hpp"
#include "ArduinoJson.hpp"
#include "Config/parameterDef.hpp"
#include "arrayFunctions.hpp"
#include "X10A/comm.hpp"
#include "X10a/converters.hpp"
#include "Config/config.hpp"
#include "debugSerial.hpp"

#define MODELS_DOC_SIZE 1024*10
#define WIFI_DOC_SIZE 1024

struct WebUIScanX10ARegister {
    int8_t PinRx;
    int8_t PinTx;
    X10AProtocol Protocol;
    String Params;
};

struct WebUIScanCANRegister {
    CAN_ICTypes CAN_IC;
    CAN_ICBus CAN_BUS;
    struct
    {
        uint8_t PIN_CAN_RX;
        uint8_t PIN_CAN_TX;
    } CAN_UART;
    struct
    {
        uint8_t PIN_MOSI;
        uint8_t PIN_MISO;
        uint8_t PIN_SCK;
        uint8_t PIN_CS;
        uint8_t PIN_INT;
        uint8_t IC_MHZ;
    } CAN_SPI;
    struct {
        String DEVICENAME;
        bool USE_PIN;
        String PIN;
    } CAN_BLUETOOTH;
    uint16_t CAN_SPEED_KBPS;
    int16_t KBPS;
    String Params;
};

enum ValueLoadState {
    NotLoading,
    Pending,
    Loading,
    LoadingFinished
};

extern WebUIScanX10ARegister webuiScanX10ARegisterConfig;
extern WebUIScanCANRegister webuiScanCANRegisterConfig;
extern ValueLoadState valueX10ALoadState;
extern ValueLoadState valueCANLoadState;
extern ValueLoadState wifiLoadState;
extern String valueX10ALoadResponse;
extern String valueCANLoadResponse;
extern String wifiLoadResponse;

void webuiScanX10ARegister();
void webuiScanCANRegister();
void webuiScanWifi();

#endif