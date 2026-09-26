#ifndef ESPALTHERMA_EVENTRING_H
#define ESPALTHERMA_EVENTRING_H

// Fixed-size history of events, oldest overwritten first. Pure logic (the native tests cover it): eventlog.h
// adds the storage (RTC memory, NVS) and the clock.

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define EVENT_TEXT_SIZE 86
#define EVENT_COUNT 24
#define EVENT_MAGIC 0x45564E54UL

struct EventEntry
{
  uint32_t epoch;  // UTC seconds, 0 while the clock was not set
  uint32_t uptime; // seconds since boot
  uint16_t boot;   // boot number
  char text[EVENT_TEXT_SIZE];
};

struct EventRing
{
  uint32_t magic;
  uint32_t head; // events added since the history started: the next goes to head % EVENT_COUNT
  EventEntry entries[EVENT_COUNT];
  uint32_t check; // over all of the above: RTC memory is random after a power cut

  void clear()
  {
    memset(this, 0, sizeof(*this));
    magic = EVENT_MAGIC;
    check = checksum();
  }

  uint32_t checksum() const
  {
    const uint8_t *p = reinterpret_cast<const uint8_t *>(this);
    uint32_t h = 2166136261UL; // FNV-1a
    for (size_t i = 0; i < offsetof(EventRing, check); i++)
    {
      h ^= p[i];
      h *= 16777619UL;
    }
    return h;
  }

  bool valid() const { return magic == EVENT_MAGIC && check == checksum(); }

  size_t count() const { return head < EVENT_COUNT ? head : EVENT_COUNT; }

  // i = 0: the oldest
  const EventEntry &at(size_t i) const { return entries[(head - count() + i) % EVENT_COUNT]; }

  void add(uint32_t epoch, uint32_t uptime, uint16_t boot, const char *text)
  {
    EventEntry &e = entries[head % EVENT_COUNT];
    e.epoch = epoch;
    e.uptime = uptime;
    e.boot = boot;
    strncpy(e.text, text, EVENT_TEXT_SIZE - 1);
    e.text[EVENT_TEXT_SIZE - 1] = 0;
    head++;
    check = checksum();
  }

  // The clock was just set: dates the events of this boot recorded before.
  void backfill(uint16_t boot, uint32_t epochNow, uint32_t uptimeNow)
  {
    for (size_t i = 0; i < count(); i++)
    {
      EventEntry &e = entries[(head - count() + i) % EVENT_COUNT];
      if (e.boot == boot && e.epoch == 0 && e.uptime <= uptimeNow)
        e.epoch = epochNow - (uptimeNow - e.uptime);
    }
    check = checksum();
  }
};

#endif
