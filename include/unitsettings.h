#ifndef ESPALTHERMA_UNITSETTINGS_H
#define ESPALTHERMA_UNITSETTINGS_H

#include "control.h"

// The installer's field settings the hydro board holds in its memory, read only, where the software is mapped for
// them. One byte each at the setting's offset (the 'I' memory read the targets use): (value - the minimum of its
// range) << 2, plus two flag bits. A setting whose range starts at another setting is stored relative to that one's
// value: the leaving water curve relative to [9-01], the cooling curve relative to [9-03]. Mapped on 20002203 by
// changing each setting on the interface, comparing the board's memory and the settings export before and after,
// and setting it back (doc/memory-20002203.md); [1-01] was not changed, its byte matches the export.
#define UNIT_SETTING_ABSOLUTE 0xFFFF
#define UNIT_SETTINGS_MAX 17
#define UNIT_SETTINGS_READ_INTERVAL_MS 600000UL // settings change rarely: a read every ten minutes

struct UnitSettingDef
{
  uint16_t offset;     // in the board's memory
  int8_t minimum;      // the minimum of its range (UNIT_SETTING_ABSOLUTE ranges only)
  uint16_t relativeTo; // the offset of the setting its range starts at, or UNIT_SETTING_ABSOLUTE
  const char *label;   // the key in MQTT and the sensor's name in Home Assistant, all in degrees Celsius
};

static const UnitSettingDef UNIT_SETTINGS_20002203[] = {
    {366, 35, UNIT_SETTING_ABSOLUTE, "Setting [0-0B]: Hot water curve, mild end (C)"},
    {367, 45, UNIT_SETTING_ABSOLUTE, "Setting [0-0C]: Hot water curve, cold end (C)"},
    {368, 10, UNIT_SETTING_ABSOLUTE, "Setting [0-0D]: Hot water curve, mild outdoor (C)"},
    {369, -40, UNIT_SETTING_ABSOLUTE, "Setting [0-0E]: Hot water curve, cold outdoor (C)"},
    {370, -40, UNIT_SETTING_ABSOLUTE, "Setting [1-00]: Heating curve, cold outdoor (C)"},
    {371, 10, UNIT_SETTING_ABSOLUTE, "Setting [1-01]: Heating curve, mild outdoor (C)"},
    {372, 0, 461, "Setting [1-02]: Heating curve, cold end (C)"},
    {373, 0, 461, "Setting [1-03]: Heating curve, mild end (C)"},
    {375, 10, UNIT_SETTING_ABSOLUTE, "Setting [1-06]: Cooling curve, mild outdoor (C)"},
    {376, 25, UNIT_SETTING_ABSOLUTE, "Setting [1-07]: Cooling curve, hot outdoor (C)"},
    {377, 0, 463, "Setting [1-08]: Cooling curve, mild end (C)"},
    {378, 0, 463, "Setting [1-09]: Cooling curve, hot end (C)"},
    {403, 14, UNIT_SETTING_ABSOLUTE, "Setting [4-02]: Heating off above outdoor (C)"},
    {460, 37, UNIT_SETTING_ABSOLUTE, "Setting [9-00]: Heating leaving water, maximum (C)"},
    {461, 15, UNIT_SETTING_ABSOLUTE, "Setting [9-01]: Heating leaving water, minimum (C)"},
    {462, 18, UNIT_SETTING_ABSOLUTE, "Setting [9-02]: Cooling leaving water, maximum (C)"},
    {463, 5, UNIT_SETTING_ABSOLUTE, "Setting [9-03]: Cooling leaving water, minimum (C)"},
};

struct UnitSettingMap
{
  const char *softwareId;
  const UnitSettingDef *defs;
  size_t count;
};

static const UnitSettingMap UNIT_SETTING_MAPS[] = {
    {"20002203", UNIT_SETTINGS_20002203, sizeof(UNIT_SETTINGS_20002203) / sizeof(UNIT_SETTINGS_20002203[0])},
};
static_assert(sizeof(UNIT_SETTINGS_20002203) / sizeof(UNIT_SETTINGS_20002203[0]) <= UNIT_SETTINGS_MAX, "room for the settings");
#define UNIT_SETTING_MAP_COUNT (sizeof(UNIT_SETTING_MAPS) / sizeof(UNIT_SETTING_MAPS[0]))

inline const UnitSettingMap *unitSettingMapFor(const char *softwareId)
{
  for (size_t i = 0; i < UNIT_SETTING_MAP_COUNT; i++)
    if (strcmp(UNIT_SETTING_MAPS[i].softwareId, softwareId) == 0)
      return &UNIT_SETTING_MAPS[i];
  return nullptr;
}

// Reads the mapped settings every ten minutes once the unit is ready (its meter read: a board just powered up reads
// 0 before it has loaded its memory). Nothing is written.
struct SettingsReader : X10aClient
{
  const UnitSettingMap *map = nullptr;
  int value[UNIT_SETTINGS_MAX] = {0};
  bool known[UNIT_SETTINGS_MAX] = {false};
  unsigned long readAt = 0;
  bool everRead = false;

  void reset()
  {
    map = nullptr;
    readAt = 0;
    everRead = false;
    for (size_t i = 0; i < UNIT_SETTINGS_MAX; i++)
      known[i] = false;
  }

  size_t count() const { return map != nullptr ? map->count : 0; }

  // A byte the board has not loaded (0) or that is no setting (0xFF): unknown
  static bool byteLoaded(uint8_t b) { return b != 0x00 && b != 0xFF; }

  int positionOf(uint16_t offset) const
  {
    for (size_t i = 0; i < count(); i++)
      if (map->defs[i].offset == offset)
        return (int)i;
    return -1;
  }

  // The absolute settings first, then those relative to one of them
  void decode(const uint8_t *raw, const bool *got)
  {
    for (int pass = 0; pass < 2; pass++)
      for (size_t i = 0; i < count(); i++)
      {
        const UnitSettingDef &d = map->defs[i];
        bool relative = d.relativeTo != UNIT_SETTING_ABSOLUTE;
        if (relative != (pass == 1))
          continue;
        int base = d.minimum;
        if (relative)
        {
          int at = positionOf(d.relativeTo);
          if (at < 0 || !known[at])
          {
            known[i] = false;
            continue;
          }
          base = value[at];
        }
        known[i] = got[i] && byteLoaded(raw[i]);
        if (known[i])
          value[i] = base + (raw[i] >> 2);
      }
  }

  void read(unsigned long now)
  {
    readAt = now;
    uint8_t raw[UNIT_SETTINGS_MAX] = {0};
    bool got[UNIT_SETTINGS_MAX] = {false};
    for (size_t i = 0; i < count(); i++)
      got[i] = readMemory(map->defs[i].offset, raw[i]);
    decode(raw, got);
    everRead = true;
  }

  // Heat pump task only. softwareId: the hydro board's, once known; unitReady: its meter read (counts loaded).
  // true when the settings became readable or stopped being: their sensors join or leave the discovery.
  bool tick(unsigned long now, const char *softwareId, bool unitReady)
  {
    bool had = map != nullptr && everRead;
    if (softwareId == nullptr || softwareId[0] == 0)
    {
      reset();
      return had;
    }
    const UnitSettingMap *m = unitSettingMapFor(softwareId);
    if (m != map)
    {
      reset();
      map = m;
    }
    if (map != nullptr && unitReady && (!everRead || now - readAt >= UNIT_SETTINGS_READ_INTERVAL_MS))
      read(now);
    return had != (map != nullptr && everRead);
  }
};

#endif
