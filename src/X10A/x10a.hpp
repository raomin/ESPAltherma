#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include "IX10ASerial.hpp"
#include "debugStream.hpp"
#include "arrayFunctions.hpp"
#include "registryBuffer.hpp"
#include "parameterDef.hpp"
#include "converters.hpp"
#include "config.hpp"

#define SER_TIMEOUT 300 // leave 300ms for the machine to answer

extern std::function<void(const ulong ms)> callbackX10A_wait;
extern std::function<void()> callbackX10A_sendValues;
extern std::function<void(ParameterDef *labelDef)> callbackX10A_updateValues;

extern size_t registryBufferSize;
extern RegistryBuffer *registryBuffers;

enum HandleState : uint8_t {
    Stopped,
    Running
};

void x10a_end();

void x10a_initRegistries(RegistryBuffer** buffer, size_t& bufferSize);

void x10a_handle(RegistryBuffer* buffer, const size_t& bufferSize, const bool sendValuesViaMQTT);

void x10a_convert_values(RegistryBuffer* buffer, const size_t& bufferSize, const bool sendValuesViaMQTT);

void x10a_init(X10A_Config* X10AConfig, const bool disableMQTTLogMessagesToInit);

void x10a_fill_config(JsonObject &jsonObject, X10A_Config *X10AConfig);

void x10a_loop();