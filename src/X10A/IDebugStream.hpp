#pragma once

#include <Arduino.h>

class IDebugStream
{
    public:
        virtual ~IDebugStream() {};

        virtual size_t printf(const char * format, ...);

        virtual size_t print(const __FlashStringHelper *ifsh);
        virtual size_t print(const String& s);
        virtual size_t print(const char c[]);
        virtual size_t print(char c);

        virtual size_t println(const __FlashStringHelper *ifsh);
        virtual size_t println(const String& s);
        virtual size_t println(const char c[]);
        virtual size_t println(char c);
        virtual size_t println(void);
};