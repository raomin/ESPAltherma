#pragma once

#include <PubSubClient.h>
#include <x10a.hpp>
#include <DebugStream.hpp>
#include <X10ASerial.hpp>

#include "CAN/canBus.hpp"
#include "arduinoC.hpp"
#include "Config/config.hpp"
#include "debugSerial.hpp"
#include "MQTT/mqtt.hpp"
#include "MQTT/mqttCallbacks.hpp"
#include "WebUI/webui.hpp"
#include "wireless.hpp"
#include "persistence.hpp"
#include "restart.hpp"

#ifdef ARDUINO_ARCH_ESP8266
#define LITTLEFS_BEGIN_PARAM
#else
#define LITTLEFS_BEGIN_PARAM true
#endif

void IRAM_ATTR restartInStandaloneWifi();

void setup();

void waitLoop(ulong ms);

void loop();