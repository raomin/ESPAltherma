#ifndef ESPALTHERMA_EVENTLOG_H
#define ESPALTHERMA_EVENTLOG_H

// Event history for postmortems: boots and restart causes, network changes, supply drops, firmware updates.
// The log (mqttserial.h) wraps within seconds; this keeps the few lines that explain an incident.
//  - In RTC memory: survives the firmware's own restarts, crashes and watchdog resets.
//  - Saved to NVS at most every 2 minutes when it changed, and before a deliberate restart: after a power cut,
//    only the last minutes are missing.
//  - Times are UTC from NTP; the events of a boot recorded before the clock was set are dated then.

#include <Arduino.h>
#include <Preferences.h>
#include <esp_attr.h>
#include <stdarg.h>
#include <time.h>
#include "eventring.h"

#define EVENT_NVS_NAMESPACE "events"
#define EVENT_SAVE_MS 120000UL
#define EVENT_CLOCK_VALID 1700000000UL // earlier: the clock is not set

RTC_NOINIT_ATTR EventRing eventRing;
portMUX_TYPE eventMux = portMUX_INITIALIZER_UNLOCKED;
uint16_t eventBoot = 0;
volatile bool eventDirty = false;
unsigned long eventSavedAt = 0;
bool eventClockSet = false;

static uint32_t eventEpoch()
{
  time_t t = time(nullptr);
  return t > (time_t)EVENT_CLOCK_VALID ? (uint32_t)t : 0;
}

// Any task (also the watchdog timer): memory only.
void eventAdd(const char *text)
{
  uint32_t epoch = eventEpoch();
  uint32_t uptime = millis() / 1000;
  portENTER_CRITICAL(&eventMux);
  eventRing.add(epoch, uptime, eventBoot, text);
  portEXIT_CRITICAL(&eventMux);
  eventDirty = true;
}

void eventAddf(const char *format, ...)
{
  char text[EVENT_TEXT_SIZE];
  va_list args;
  va_start(args, format);
  vsnprintf(text, sizeof(text), format, args);
  va_end(args);
  eventAdd(text);
}

// Consistent copy, for the web interface and the NVS.
void eventCopy(EventRing &out)
{
  portENTER_CRITICAL(&eventMux);
  memcpy(&out, &eventRing, sizeof(out));
  portEXIT_CRITICAL(&eventMux);
}

// Main loop only (NVS).
void eventSave()
{
  static EventRing copy;
  eventDirty = false;
  eventCopy(copy);
  Preferences prefs;
  if (prefs.begin(EVENT_NVS_NAMESPACE, false))
  {
    prefs.putBytes("ring", &copy, sizeof(copy));
    prefs.end();
  }
  eventSavedAt = millis();
}

// First thing at boot: numbers the boot, and recovers the history saved before a power cut.
void eventBegin()
{
  Preferences prefs;
  prefs.begin(EVENT_NVS_NAMESPACE, false);
  eventBoot = prefs.getUShort("boot", 0) + 1;
  prefs.putUShort("boot", eventBoot);
  if (!eventRing.valid())
  { // RTC memory lost (power cut)
    if (prefs.getBytesLength("ring") != sizeof(EventRing) || prefs.getBytes("ring", &eventRing, sizeof(EventRing)) != sizeof(EventRing) || !eventRing.valid())
      eventRing.clear();
  }
  prefs.end();
}

// Main loop: dates the first events once the clock is set, saves when due.
void eventLoop()
{
  if (!eventClockSet)
  {
    uint32_t epoch = eventEpoch();
    if (epoch != 0)
    {
      eventClockSet = true;
      portENTER_CRITICAL(&eventMux);
      eventRing.backfill(eventBoot, epoch, millis() / 1000);
      portEXIT_CRITICAL(&eventMux);
      eventDirty = true;
    }
  }
  if (eventDirty && millis() - eventSavedAt >= EVENT_SAVE_MS)
    eventSave();
}

#endif
