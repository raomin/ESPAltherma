#pragma once

#include <Arduino.h>

class IX10ASerial
{
    public:
        virtual ~IX10ASerial() = default;

        virtual void begin(uint64_t baud, int8_t rxPin, int8_t txPin) = 0;
        virtual void end() = 0;

        virtual void flush() = 0;

        virtual void write(const uint8_t *, size_t) = 0;

        virtual bool available() = 0;
        virtual int read() = 0;

        operator bool() const;
};