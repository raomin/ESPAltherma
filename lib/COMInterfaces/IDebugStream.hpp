#pragma once

#include <Arduino.h>

class IDebugStream
{
    public:
        virtual ~IDebugStream() = default;

        virtual size_t printf(const char * format, ...) = 0;

        virtual size_t print(const __FlashStringHelper *ifsh) = 0;
        virtual size_t print(const String& s) = 0;
        virtual size_t print(const char c[]) = 0;
        virtual size_t print(char c) = 0;

        virtual size_t println(const __FlashStringHelper *ifsh) = 0;
        virtual size_t println(const String& s) = 0;
        virtual size_t println(const char c[]) = 0;
        virtual size_t println(char c) = 0;
        virtual size_t println(void) = 0;
};