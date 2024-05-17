#include "DebugStream.hpp"

#ifndef PIO_UNIT_TESTING

DebugStream::DebugStream(Stream* stream)
{
    this->stream = stream;
}

size_t DebugStream::printf(const char * format, ...)
{
    va_list arg;
    va_start(arg, format);
    size_t res = stream->printf(format, arg);
    va_end(arg);
    return res;
}

inline size_t DebugStream::print(const __FlashStringHelper *ifsh)
{
    return stream->print(ifsh);
}

inline size_t DebugStream::print(const String& s)
{
    return stream->print(s);
}

inline size_t DebugStream::print(const char c[])
{
    return stream->print(c);
}

inline size_t DebugStream::print(char c)
{
    return stream->print(c);
}

inline size_t DebugStream::println(const __FlashStringHelper *ifsh)
{
    return stream->println(ifsh);
}

inline size_t DebugStream::println(const String& s)
{
    return stream->println(s);
}

inline size_t DebugStream::println(const char c[])
{
    return stream->println(c);
}

inline size_t DebugStream::println(char c)
{
    return stream->println(c);
}

inline size_t DebugStream::println(void)
{
    return stream->println();
}

#endif