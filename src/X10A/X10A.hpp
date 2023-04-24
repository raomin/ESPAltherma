#ifndef X10A_H
#define X10A_H

#include <HardwareSerial.h>
#ifdef ARDUINO_ARCH_ESP8266
#include <SoftwareSerial.h>
#endif

#include "RegistryBuffer.hpp"
#include "Config/config.hpp"
#include "debugSerial.hpp"
#include "MQTT/mqtt.hpp"
#include "arrayFunctions.hpp"
#include "comm.hpp"
#include "main.hpp"

extern HardwareSerial SerialX10A;
extern size_t registryBufferSize;
extern RegistryBuffer *registryBuffers;

void X10AEnd();

void initRegistries(RegistryBuffer** buffer, size_t& bufferSize, ParameterDef** parameters, const size_t parametersLength);

void handleX10A(RegistryBuffer* buffer, const size_t& bufferSize, ParameterDef** parameters, const size_t parametersLength, const bool sendValuesViaMQTT, X10AProtocol protocol);

void X10AInit(int8_t rxPin, int8_t txPin);

#endif