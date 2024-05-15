#pragma once

#ifndef PIO_UNIT_TESTING

#include "IX10ASerial.hpp"

#ifdef ARDUINO_ARCH_ESP8266
#include <SoftwareSerial.h>
#define SERIAL_TYPE SoftwareSerial
#define SERIAL_CONFIG (SWSERIAL_8E1)
#define SERIAL_FLUSH_TX_ONLY // empty, as SoftwareSerial.flush() takes no parameter
#define DEFINE_SerialX10A SERIAL_TYPE SerialX10A
#else
#include <HardwareSerial.h>
#define SERIAL_TYPE HardwareSerial
#define SERIAL_CONFIG (SERIAL_8E1)
#define SERIAL_FLUSH_TX_ONLY false
#define DEFINE_SerialX10A SERIAL_TYPE SerialX10A(1)
#endif

class X10ASerial : public IX10ASerial
{
    public:
        void begin(uint64_t baud, int8_t rxPin, int8_t txPin);
        void end();

        void flush();

        void write(const uint8_t * data, size_t length);

        bool available();
        int read();

        operator bool() const;
};

#endif