#pragma once

#include <Arduino.h>
#include "IDebugStream.hpp"

class DebugStream : public IDebugStream
{
    private:
        Stream* stream;

    public:
        DebugStream(Stream* stream);

        size_t printf(const char * format, ...);

        size_t print(const __FlashStringHelper *ifsh);
        size_t print(const String& s);
        size_t print(const char c[]);
        size_t print(char c);

        size_t println(const __FlashStringHelper *ifsh);
        size_t println(const String& s);
        size_t println(const char c[]);
        size_t println(char c);
        size_t println(void);
};