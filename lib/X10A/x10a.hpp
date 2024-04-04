#ifndef X10A_H
#define X10A_H

#ifdef PIO_UNIT_TESTING
#include <unity.h>
#include <SerialFake.h>
typedef unsigned long ulong;
extern SerialFake SerialX10A;
#define SERIAL_CONFIG (SWSERIAL_8E1)
#define DEFINE_SerialX10A SerialFake SerialX10A
#else

#ifdef ARDUINO_ARCH_ESP8266
#include <SoftwareSerial.h>
extern SoftwareSerial SerialX10A;
#define SERIAL_CONFIG (SWSERIAL_8E1)
#define DEFINE_SerialX10A SoftwareSerial SerialX10A
#else
#include <HardwareSerial.h>
extern HardwareSerial SerialX10A;
#define SERIAL_CONFIG (SERIAL_8E1)
#define DEFINE_SerialX10A HardwareSerial SerialX10A(1)
#endif
#endif

#include "debugStream.hpp"
#include "arrayFunctions.hpp"
#include "registryBuffer.hpp"
#include "parameterDef.hpp"
#include "converters.hpp"
#include "config.hpp"
#include "comm.hpp"

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

void x10a_loop();

#endif