#pragma once

#include <ArduinoFake.h>
#include <IDebugStream.hpp>

class MockDebugStream : public IDebugStream
{
    public:
        ~MockDebugStream() {};

        size_t printf(const char * format, ...) { return 0; }

        size_t print(const __FlashStringHelper *ifsh) { return 0; }
        size_t print(const String& s) { return 0; }
        size_t print(const char c[]) { return 0; }
        size_t print(char c) { return 0; }

        size_t println(const __FlashStringHelper *ifsh) { return 0; }
        size_t println(const String& s)  { return 0; }
        size_t println(const char c[]) { return 0; }
        size_t println(char c) { return 0; }
        size_t println(void) { return 0; }
};