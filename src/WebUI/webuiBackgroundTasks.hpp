#ifndef WEBUI_BACKGROUND_TASKS_H
#define WEBUI_BACKGROUND_TASKS_H

#include <ArduinoJson.hpp>
#include "CAN/canBus.hpp"
#include "X10A/x10a.hpp"
#include "arduinoC.hpp"
#include "Config/config.hpp"
#include "debugSerial.hpp"
#include "wireless.hpp"

#define MODELS_DOC_SIZE 1024*10
#define WIFI_DOC_SIZE 1024

#ifdef ARDUINO_ARCH_ESP8266
#define WIFI_TYPE_OPEN ENC_TYPE_NONE
#else
#define WIFI_TYPE_OPEN WIFI_AUTH_OPEN
#endif

enum ValueLoadState {
    NotLoading,
    Pending,
    Loading,
    LoadingFinished
};

extern X10A_Config* webuiScanX10ARegisterConfig;
extern CAN_Config* webuiScanCANRegisterConfig;
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
