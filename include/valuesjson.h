#ifndef ESPALTHERMA_VALUESJSON_H
#define ESPALTHERMA_VALUESJSON_H

// The values as the web page gets them in its "values" event: [{"l":label,"v":value,"r":registry},...], the text
// ArduinoJson's serializeJson() gave. Written straight into a buffer of the size it needs, in place of a JsonDocument
// and of a String that grew as it was serialized: with 128 values (6 KB of text) and a page open, publishing a poll
// took 39 KB of the heap, 26 KB now (an ESP32-S3, the median of twelve polls; 9 KB and 8 KB with no page open).
//
// No Arduino dependency: tested natively against serializeJson().

#include <stdio.h>
#include <string.h>
#include "labeldef.h"

// Appends one character to out while there is room; counts it in any case
inline void valuesJsonPut(char c, char *out, size_t size, size_t &length)
{
  if (out != nullptr && length + 1 < size)
    out[length] = c;
  length++;
}

inline void valuesJsonPutText(const char *text, char *out, size_t size, size_t &length)
{
  for (; *text; text++)
    valuesJsonPut(*text, out, size, length);
}

// A JSON string, escaped as serializeJson() does it: quote, backslash and control characters; bytes from 0x80 on as
// they are (UTF-8, or whatever the definition holds)
inline void valuesJsonPutString(const char *text, char *out, size_t size, size_t &length)
{
  valuesJsonPut('"', out, size, length);
  for (const unsigned char *c = (const unsigned char *)(text != nullptr ? text : ""); *c; c++)
  {
    const char *named = nullptr;
    switch (*c)
    {
    case '"': named = "\\\""; break;
    case '\\': named = "\\\\"; break;
    case '\b': named = "\\b"; break;
    case '\f': named = "\\f"; break;
    case '\n': named = "\\n"; break;
    case '\r': named = "\\r"; break;
    case '\t': named = "\\t"; break;
    }
    if (named != nullptr)
      valuesJsonPutText(named, out, size, length);
    else if (*c < 0x20)
    {
      char escaped[8];
      snprintf(escaped, sizeof(escaped), "\\u%04x", *c);
      valuesJsonPutText(escaped, out, size, length);
    }
    else
      valuesJsonPut((char)*c, out, size, length);
  }
  valuesJsonPut('"', out, size, length);
}

// Writes the values into out (size bytes, zero-terminated when there is room for anything) and returns the length of
// the whole text, as snprintf() does: called with out == nullptr it says the room to take, which is that plus one.
inline size_t valuesJson(const LabelDef *labels, size_t count, char *out, size_t size)
{
  size_t length = 0;
  valuesJsonPut('[', out, size, length);
  for (size_t i = 0; i < count; i++)
  {
    if (i > 0)
      valuesJsonPut(',', out, size, length);
    valuesJsonPutText("{\"l\":", out, size, length);
    valuesJsonPutString(labels[i].label, out, size, length);
    valuesJsonPutText(",\"v\":", out, size, length);
    valuesJsonPutString(labels[i].asString, out, size, length);
    char registry[24];
    snprintf(registry, sizeof(registry), ",\"r\":%d}", labels[i].registryID);
    valuesJsonPutText(registry, out, size, length);
  }
  valuesJsonPut(']', out, size, length);
  if (out != nullptr && size > 0)
    out[length < size ? length : size - 1] = 0;
  return length;
}

#endif
