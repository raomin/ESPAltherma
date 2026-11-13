#ifndef ESPALTHERMA_VALUEQUALITY_H
#define ESPALTHERMA_VALUEQUALITY_H

#include <stdint.h>
#include <stddef.h>

enum ValueQuality : uint8_t { VALUE_NEVER, VALUE_OK, VALUE_READ_FAILED, VALUE_MISSING, VALUE_STALE };
inline const char *valueQualityName(ValueQuality q)
{
  switch (q)
  {
    case VALUE_OK: return "ok";
    case VALUE_READ_FAILED: return "read_failed";
    case VALUE_MISSING: return "missing";
    case VALUE_STALE: return "stale";
    default: return "never";
  }
}

// One timestamp per selected registry, not per sensor. 32 entries use 256 bytes on ESP32.
class ValueQualityTracker
{
  struct Entry { uint32_t lastGood = 0; uint8_t reg = 0, seen = 0, good = 0; };
  Entry entries[32];
  uint8_t count = 0;
public:
  void reset() { count = 0; }

  void note(uint8_t reg, bool good, uint32_t now)
  {
    Entry *e = nullptr;
    for (uint8_t i = 0; i < count; i++)
      if (entries[i].reg == reg) e = &entries[i];
    if (!e && count < 32) { e = &entries[count++]; *e = Entry(); e->reg = reg; }
    if (!e) return;
    e->seen |= good ? 2 : 1;
    e->good = good;
    if (good) e->lastGood = now;
  }

  static uint32_t maxAge(uint32_t period)
  {
    // 3 poll periods, at least a minute; saturating multiplication handles arbitrary saved config values.
    uint32_t age = period > UINT32_MAX / 3 ? UINT32_MAX : period * 3;
    return age < 60000 ? 60000 : age;
  }

  ValueQuality quality(uint8_t reg, bool fieldValid, uint32_t now, uint32_t period) const
  {
    for (uint8_t i = 0; i < count; i++)
    {
      const Entry &e = entries[i];
      if (e.reg != reg) continue;
      if (!e.good) return VALUE_READ_FAILED;
      if ((uint32_t)(now - e.lastGood) >= maxAge(period)) return VALUE_STALE;
      return fieldValid ? VALUE_OK : VALUE_MISSING;
    }
    return VALUE_NEVER;
  }

  int32_t ageSeconds(uint8_t reg, uint32_t now) const
  {
    for (uint8_t i = 0; i < count; i++)
      if (entries[i].reg == reg && (entries[i].seen & 2))
        return (int32_t)((uint32_t)(now - entries[i].lastGood) / 1000);
    return -1;
  }
};

#endif
