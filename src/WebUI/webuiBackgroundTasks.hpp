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

enum ValueLoadState {
    NotLoading,
    Pending,
    Loading,
    LoadingFinished
};

extern WebUIScanX10ARegister webuiScanX10ARegisterConfig;
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