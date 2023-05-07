#ifndef WEBUI_BACKGROUND_TASKS_H
#define WEBUI_BACKGROUND_TASKS_H

#include "ArduinoC.hpp"
#include "ArduinoJson.hpp"
#include "arrayFunctions.hpp"
#include "X10A/comm.hpp"
#include "X10A/converters.hpp"
#include "X10A/parameterDef.hpp"
#include "X10A/X10A.hpp"
#include "X10A/config.hpp"
#include "Config/config.hpp"
#include "CAN/canBus.hpp"
#include "debugSerial.hpp"

#define MODELS_DOC_SIZE 1024*10
#define WIFI_DOC_SIZE 1024

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
