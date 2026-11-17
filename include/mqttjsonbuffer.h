#ifndef ESPALTHERMA_MQTTJSONBUFFER_H
#define ESPALTHERMA_MQTTJSONBUFFER_H
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

inline bool mqttJsonAppend(char *out,size_t size,bool &failed,const char *format,...)
{
  if(failed || !out || !size) {failed=true;return false;}
  size_t used=strnlen(out,size);
  if(used>=size) {failed=true;return false;}
  va_list args;va_start(args,format);int n=vsnprintf(out+used,size-used,format,args);va_end(args);
  if(n<0 || (size_t)n>=size-used) failed=true;
  return !failed;
}

// The room a value takes in the message: "name":"text", (a number goes without its quotes, two bytes less)
inline size_t mqttJsonValueRoom(const char *name,const char *text)
{
  return strlen(name)+strlen(text)+6;
}

// The room of the message around its values: "[{" before, "}]" in place of the last comma, and the final zero
#define MQTT_JSON_FRAME_ROOM 4

// The buffer is taken in steps of this size. The room follows the texts of the values and moves by a few bytes
// from poll to poll: a buffer a little larger than the last one cannot reuse the hole it left in the heap, one of
// the same size can.
#define MQTT_JSON_ROOM_STEP 1024

inline size_t mqttJsonRoomRounded(size_t room)
{
  return (room + MQTT_JSON_ROOM_STEP - 1) / MQTT_JSON_ROOM_STEP * MQTT_JSON_ROOM_STEP;
}

inline bool mqttJsonFinish(char *out,size_t size,bool table,bool failed)
{
  if(failed || !out || !size) return false;
  size_t used=strnlen(out,size);
  // Legacy diagnostic appenders also use bounded snprintf: saturation means possible truncation.
  if(used>=size-1 || used<(table?2u:1u)) return false;
  if(out[used-1]==',') out[--used]=0;
  size_t needed=table?2:1;
  if(needed>=size-used) return false;
  out[used++]='}';if(table) out[used++]=']';out[used]=0;
  return true;
}
#endif
