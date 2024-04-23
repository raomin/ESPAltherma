#pragma once

#include <Arduino.h>

class IX10ASerial
{
    public:
        virtual void begin(ulong baud, int8_t rxPin, int8_t txPin);
        virtual void end();

        virtual void flush();

        virtual void write(const uint8_t *, size_t);

        virtual bool available();
        virtual int read();
};