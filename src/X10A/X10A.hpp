#ifndef X10A_H
#define X10A_H

#include <HardwareSerial.h>
#ifdef ARDUINO_ARCH_ESP8266
#include <SoftwareSerial.h>
#endif

#include "registryBuffer.hpp"
#include "Config/config.hpp"
#include "debugSerial.hpp"
#include "MQTT/mqtt.hpp"
#include "arrayFunctions.hpp"
#include "config.hpp"
#include "comm.hpp"
#include "main.hpp"

extern HardwareSerial SerialX10A;
extern size_t registryBufferSize;
extern RegistryBuffer *registryBuffers;

enum HandleState : uint8_t {
    Stopped,
    Running
};

void X10AEnd();

void initRegistries(RegistryBuffer** buffer, size_t& bufferSize);

void handleX10A(RegistryBuffer* buffer, const size_t& bufferSize, const bool sendValuesViaMQTT);

void X10AInit(X10A_Config* X10AConfig);

void X10A_loop();

#endif