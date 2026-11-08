#ifndef ESPALTHERMA_ONEWIRE_TEMP_H
#define ESPALTHERMA_ONEWIRE_TEMP_H

// DS18B20 temperature sensors on spare GPIOs of the board (1-Wire): the tank layers, the floor loop, a wood
// stove circuit... read next to the heat pump, so that one board does the job of two.
//
// Each configured data GPIO is a 1-Wire bus (a 4.7 kOhm pull-up to the sensors' supply, the sensors powered,
// not parasitic). The buses are searched at start and on request; every reading cycle starts a conversion on all
// sensors of a bus at once (skip ROM + 0x44), and reads each one's scratchpad 800 ms later, so the main loop is
// never held for the conversion time. A reading is kept only when its CRC is right; 85.0 °C, the value a sensor
// holds after a power-on, is kept only when the previous reading was close to it (a tank can be at 85 °C).
// A sensor that fails three cycles in a row reads null until it answers again.
//
// Each bus can take its supply and its ground from GPIOs instead of 3.3 V and GND (optional, so that VDD, DATA
// and GND sit next to each other on a connector). Then a bus where no sensor answers any more is switched off
// for a second and searched again: a DS18B20 hung by a glitch on a long cable comes back. Only that bus, at most
// every 10 minutes, so a bus that is really empty is not cycled all the time.
//
// No Arduino dependency: the bus is an interface, so a unit test drives it with fake sensors.

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#define ONEWIRE_MAX_BUSES 4
#define ONEWIRE_MAX_SENSORS 16
#define ONEWIRE_MAX_NAMES 16
#define ONEWIRE_NAME_SIZE 24
#define ONEWIRE_LABEL_PREFIX "DS18B20: "
#define ONEWIRE_LABEL_SIZE (sizeof(ONEWIRE_LABEL_PREFIX) + ONEWIRE_NAME_SIZE)
#define ONEWIRE_CONVERSION_MS 800UL       // 750 ms at 12 bits, with a margin
#define ONEWIRE_FAILS_BEFORE_NULL 3
#define ONEWIRE_POWER_ON_RAW 0x0550       // 85.0 °C
#define ONEWIRE_POWER_ON_TRUSTED_WITHIN 5.0f
#define ONEWIRE_POWER_OFF_MS 1000UL          // supply off this long, so that every sensor really restarts
#define ONEWIRE_POWER_UP_MS 50UL             // after the supply is back, before the search
#define ONEWIRE_POWER_CYCLE_EVERY_MS 600000UL // at most this often

#define ONEWIRE_FAMILY_DS18B20 0x28
#define ONEWIRE_FAMILY_DS1822 0x22
#define ONEWIRE_CMD_CONVERT 0x44
#define ONEWIRE_CMD_READ_SCRATCHPAD 0xBE

// What a 1-Wire bus can do (OneWireProtocol below builds it on the bit slots of onewire_esp.h; a test, on fakes)
struct OneWireBus
{
  virtual ~OneWireBus() {}
  virtual bool reset() = 0; // true when a sensor answered the reset pulse
  virtual void skip() = 0;
  virtual void select(const uint8_t rom[8]) = 0;
  virtual void write(uint8_t b) = 0;
  virtual uint8_t read() = 0;
  virtual void resetSearch() = 0;
  virtual bool search(uint8_t rom[8]) = 0;
};

// The supply of the sensors from a GPIO (onewire_esp.h): off also pulls the data lines low, else the sensors
// would go on running from the pull-up
struct OneWirePower
{
  virtual ~OneWirePower() {}
  virtual void set(bool on) = 0;
};

// The three timed slots of the bus (onewire_esp.h drives them on a GPIO): everything else is built on them
struct OneWireBits
{
  virtual ~OneWireBits() {}
  virtual bool resetPulse() = 0; // true when a sensor answered with a presence pulse
  virtual void writeBit(bool bit) = 0;
  virtual bool readBit() = 0;
};

#define ONEWIRE_CMD_SEARCH_ROM 0xF0
#define ONEWIRE_CMD_MATCH_ROM 0x55
#define ONEWIRE_CMD_SKIP_ROM 0xCC

// Bytes, addressing and the ROM search (Maxim application note 187) over the bit slots
struct OneWireProtocol : OneWireBus
{
  OneWireBits *bits = nullptr;
  uint8_t romNo[8] = {0};
  int lastDiscrepancy = 0;
  bool lastDevice = false;

  bool reset() override { return bits->resetPulse(); }

  void write(uint8_t b) override
  {
    for (int i = 0; i < 8; i++)
      bits->writeBit((b >> i) & 1); // least significant bit first
  }

  uint8_t read() override
  {
    uint8_t b = 0;
    for (int i = 0; i < 8; i++)
      if (bits->readBit())
        b |= (uint8_t)(1 << i);
    return b;
  }

  void skip() override { write(ONEWIRE_CMD_SKIP_ROM); }

  void select(const uint8_t rom[8]) override
  {
    write(ONEWIRE_CMD_MATCH_ROM);
    for (int i = 0; i < 8; i++)
      write(rom[i]);
  }

  void resetSearch() override
  {
    lastDiscrepancy = 0;
    lastDevice = false;
    memset(romNo, 0, sizeof(romNo));
  }

  // The next ROM code on the bus: at each bit, every sensor still in the search sends its bit and its
  // complement; both 0 means sensors differ there, and the branch not taken yet is taken on a later call.
  bool search(uint8_t rom[8]) override
  {
    if (lastDevice || !reset())
    {
      resetSearch();
      return false;
    }
    write(ONEWIRE_CMD_SEARCH_ROM);
    int bitNumber = 1, lastZero = 0;
    for (int byteIndex = 0; byteIndex < 8; byteIndex++)
    {
      for (uint8_t mask = 1; mask != 0; mask <<= 1, bitNumber++)
      {
        bool bit = bits->readBit(), complement = bits->readBit();
        if (bit && complement)
        {
          resetSearch(); // nobody left in the search
          return false;
        }
        bool direction;
        if (bit != complement)
          direction = bit; // all remaining sensors agree on this bit
        else if (bitNumber < lastDiscrepancy)
          direction = (romNo[byteIndex] & mask) != 0; // the path of the previous search
        else
          direction = bitNumber == lastDiscrepancy;   // the other branch, this time
        if (!direction && bit == complement)
          lastZero = bitNumber;
        if (direction)
          romNo[byteIndex] |= mask;
        else
          romNo[byteIndex] &= (uint8_t)~mask;
        bits->writeBit(direction);
      }
    }
    lastDiscrepancy = lastZero;
    lastDevice = lastDiscrepancy == 0;
    memcpy(rom, romNo, 8);
    return true;
  }
};

// The CRC of the ROM codes and scratchpads (Dallas/Maxim, x^8 + x^5 + x^4 + 1, reflected)
static inline uint8_t oneWireCrc8(const uint8_t *data, size_t len)
{
  uint8_t crc = 0;
  for (size_t i = 0; i < len; i++)
  {
    uint8_t in = data[i];
    for (int bit = 0; bit < 8; bit++)
    {
      uint8_t mix = (crc ^ in) & 0x01;
      crc >>= 1;
      if (mix)
        crc ^= 0x8C;
      in >>= 1;
    }
  }
  return crc;
}

// ROM code as 16 hex digits, family first ("28FF641E8216034C")
static inline void oneWireRomToHex(const uint8_t rom[8], char out[17])
{
  for (int i = 0; i < 8; i++)
    snprintf(out + 2 * i, 3, "%02X", rom[i]);
}

static inline bool oneWireHexToRom(const char *hex, uint8_t rom[8])
{
  if (hex == nullptr || strlen(hex) != 16)
    return false;
  auto nibble = [](char c) -> int {
    return c >= '0' && c <= '9' ? c - '0' : c >= 'A' && c <= 'F' ? c - 'A' + 10 : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
  };
  for (int i = 0; i < 8; i++)
  {
    int hi = nibble(hex[2 * i]), lo = nibble(hex[2 * i + 1]);
    if (hi < 0 || lo < 0)
      return false;
    rom[i] = (uint8_t)(hi << 4 | lo);
  }
  return true;
}

static inline bool oneWireIsTemperatureSensor(const uint8_t rom[8])
{
  bool family = rom[0] == ONEWIRE_FAMILY_DS18B20 || rom[0] == ONEWIRE_FAMILY_DS1822;
  return family && oneWireCrc8(rom, 7) == rom[7];
}

// The GPIOs of one bus: its data line, and optionally its supply and ground (-1: 3.3 V and GND)
struct OneWireBusPins
{
  int8_t data = -1;
  int8_t power = -1;
  int8_t ground = -1;

  bool used() const { return data >= 0; }
};

// What a sensor is fitted to, for the drawing on the home page ("tank0" bottom … "tank10" top, "hp_flow", "iu_gas"...).
// The firmware only keeps it: lower case letters, digits and "_".
#define ONEWIRE_ROLE_SIZE 12

static inline bool oneWireValidRole(const char *role)
{
  size_t n = strlen(role);
  if (n >= ONEWIRE_ROLE_SIZE)
    return false;
  for (size_t i = 0; i < n; i++)
  {
    char c = role[i];
    bool allowed = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
    if (!allowed)
      return false;
  }
  return true;
}

// A name and a place given in the settings to a sensor, by its ROM code
struct OneWireName
{
  char rom[17];
  char name[ONEWIRE_NAME_SIZE];
  char role[ONEWIRE_ROLE_SIZE];

  bool isEmpty() const { return !name[0] && !role[0]; }
};

struct OneWireSensor
{
  uint8_t rom[8];
  uint8_t bus;
  bool valid = false;  // a reading is there
  float celsius = NAN;
  uint8_t fails = 0;   // cycles in a row without a good reading
  char label[ONEWIRE_LABEL_SIZE]; // "DS18B20: <name>" (the ROM code when unnamed): the key in MQTT and Home Assistant
};

enum OneWirePhase
{
  ONEWIRE_IDLE,
  ONEWIRE_CONVERTING,
  ONEWIRE_POWERED_OFF,
  ONEWIRE_POWERING_UP,
};

struct OneWireTemps
{
  OneWireBus *buses[ONEWIRE_MAX_BUSES] = {nullptr};
  int pins[ONEWIRE_MAX_BUSES] = {-1, -1, -1, -1}; // the GPIO of each bus, for the web page (set by the caller)
  size_t busCount = 0;
  OneWireSensor sensors[ONEWIRE_MAX_SENSORS];
  size_t count = 0;
  const OneWireName *names = nullptr;
  size_t nameCount = 0;
  unsigned long interval = 30000; // a reading cycle this often
  OneWirePhase phase = ONEWIRE_IDLE;
  unsigned long convertedAt = 0, cycleAt = 0;
  bool started = false;
  bool changed = false; // the list of sensors changed: the caller updates the Home Assistant discovery
  OneWirePower *power[ONEWIRE_MAX_BUSES] = {nullptr}; // each bus's supply when it comes from a GPIO (set by the caller)
  unsigned restartedBuses = 0;   // the buses switched off, as a bit mask
  unsigned long poweredAt = 0;   // when a supply was last switched off or on
  bool powerCycled = false;      // poweredAt is set
  unsigned powerCycles = 0;      // for the log and the web page

  void attach(OneWireBus *const *list, size_t n)
  {
    busCount = n > ONEWIRE_MAX_BUSES ? ONEWIRE_MAX_BUSES : n;
    for (size_t i = 0; i < busCount; i++)
      buses[i] = list[i];
    count = 0;
    phase = ONEWIRE_IDLE;
    started = false;
    changed = true;
  }

  void setNames(const OneWireName *list, size_t n)
  {
    names = list;
    nameCount = n;
    for (size_t i = 0; i < count; i++)
      makeLabel(i);
    changed = true;
  }

  const OneWireName *settingsFor(const uint8_t rom[8]) const
  {
    char hex[17];
    oneWireRomToHex(rom, hex);
    for (size_t i = 0; i < nameCount; i++)
      if (strcasecmp(names[i].rom, hex) == 0)
        return &names[i];
    return nullptr;
  }

  const char *nameFor(const uint8_t rom[8]) const
  {
    const OneWireName *settings = settingsFor(rom);
    return settings && settings->name[0] ? settings->name : nullptr;
  }

  const char *roleFor(const uint8_t rom[8]) const
  {
    const OneWireName *settings = settingsFor(rom);
    return settings ? settings->role : "";
  }

  // The label of sensor i: "DS18B20: <name>", the ROM code without a name. A name an earlier sensor already goes
  // by gets the end of the ROM code added: one key per sensor in MQTT, one entity in Home Assistant.
  void makeLabel(size_t i)
  {
    OneWireSensor &s = sensors[i];
    const char *name = nameFor(s.rom);
    char hex[17];
    oneWireRomToHex(s.rom, hex);
    writeLabel(s, name ? name : hex, "");
    if (name && labelTakenBefore(i))
      writeLabel(s, name, hex + 12);
  }

  static void writeLabel(OneWireSensor &s, const char *text, const char *suffix)
  {
    int room = ONEWIRE_NAME_SIZE - 1 - (suffix[0] ? (int)strlen(suffix) + 1 : 0);
    snprintf(s.label, sizeof(s.label), "%s%.*s%s%s", ONEWIRE_LABEL_PREFIX, room, text, suffix[0] ? " " : "", suffix);
    for (char *c = s.label; *c; c++)
      if (*c == '"' || *c == '\\' || *c == '\'' || (unsigned char)*c < 0x20)
        *c = ' '; // the label is a JSON key and sits between quotes in the Home Assistant template
  }

  bool labelTakenBefore(size_t i) const
  {
    for (size_t j = 0; j < i; j++)
      if (strcmp(sensors[j].label, sensors[i].label) == 0)
        return true;
    return false;
  }

  // Finds the sensors of every bus, in bus order. A sensor already known keeps its last reading.
  void search()
  {
    OneWireSensor before[ONEWIRE_MAX_SENSORS];
    size_t beforeCount = count;
    memcpy(before, sensors, sizeof(OneWireSensor) * count);
    count = 0;
    for (size_t b = 0; b < busCount; b++)
    {
      uint8_t rom[8];
      buses[b]->resetSearch();
      while (count < ONEWIRE_MAX_SENSORS && buses[b]->search(rom))
      {
        if (!oneWireIsTemperatureSensor(rom))
          continue;
        OneWireSensor &s = sensors[count++];
        s = OneWireSensor();
        memcpy(s.rom, rom, 8);
        s.bus = (uint8_t)b;
        for (size_t i = 0; i < beforeCount; i++)
          if (memcmp(before[i].rom, rom, 8) == 0)
          {
            s.valid = before[i].valid;
            s.celsius = before[i].celsius;
          }
        makeLabel(count - 1);
      }
    }
    changed = true;
  }

  // Decodes a scratchpad: false when the CRC is wrong or the value is a power-on leftover
  static bool decode(const uint8_t pad[9], bool hadReading, float previous, float &out)
  {
    if (oneWireCrc8(pad, 8) != pad[8])
      return false;
    bool allZero = true;
    for (int i = 0; i < 9; i++)
      allZero = allZero && pad[i] == 0;
    if (allZero)
      return false; // nothing answered (an all-zero frame has a zero CRC)
    int16_t raw = (int16_t)(pad[1] << 8 | pad[0]);
    float c = raw / 16.0f;
    if (raw == ONEWIRE_POWER_ON_RAW && !(hadReading && fabsf(previous - c) <= ONEWIRE_POWER_ON_TRUSTED_WITHIN))
      return false;
    out = c;
    return true;
  }

  void startConversions()
  {
    for (size_t b = 0; b < busCount; b++)
    {
      if (!buses[b]->reset())
        continue;
      buses[b]->skip();
      buses[b]->write(ONEWIRE_CMD_CONVERT);
    }
  }

  void readOne(OneWireSensor &s)
  {
    OneWireBus *bus = buses[s.bus];
    uint8_t pad[9];
    float c;
    bool ok = bus->reset();
    if (ok)
    {
      bus->select(s.rom);
      bus->write(ONEWIRE_CMD_READ_SCRATCHPAD);
      for (int i = 0; i < 9; i++)
        pad[i] = bus->read();
      ok = decode(pad, s.valid, s.celsius, c);
    }
    if (ok)
    {
      s.celsius = c;
      s.valid = true;
      s.fails = 0;
      return;
    }
    if (s.fails < 255)
      s.fails++;
    if (s.fails >= ONEWIRE_FAILS_BEFORE_NULL)
      s.valid = false;
  }

  // Runs the cycle: search once, then a conversion every interval and the reads once it is done. Main loop.
  void tick(unsigned long now)
  {
    if (busCount == 0)
      return;
    if (!started)
    {
      started = true;
      search();
      cycleAt = now - interval; // the first reading right away
    }
    if (phase == ONEWIRE_IDLE && now - cycleAt >= interval)
    {
      cycleAt = now;
      startConversions();
      convertedAt = now;
      phase = ONEWIRE_CONVERTING;
    }
    else if (phase == ONEWIRE_CONVERTING && now - convertedAt >= ONEWIRE_CONVERSION_MS)
    {
      for (size_t i = 0; i < count; i++)
        readOne(sensors[i]);
      phase = ONEWIRE_IDLE;
      if (powerCycleDue(now))
        powerOff(now);
    }
    else if (phase == ONEWIRE_POWERED_OFF && now - poweredAt >= ONEWIRE_POWER_OFF_MS)
      powerOn(now);
    else if (phase == ONEWIRE_POWERING_UP && now - poweredAt >= ONEWIRE_POWER_UP_MS)
    {
      search();
      phase = ONEWIRE_IDLE;
      cycleAt = now - interval; // read right away
    }
  }

  bool noSensorAnswersOn(size_t bus) const
  {
    for (size_t i = 0; i < count; i++)
      if (sensors[i].bus == bus && sensors[i].fails < ONEWIRE_FAILS_BEFORE_NULL)
        return false;
    return true; // also when the search found nothing: a hung sensor does not answer it either
  }

  // The buses with their own supply where no sensor answers, as a bit mask
  unsigned busesToRestart() const
  {
    unsigned mask = 0;
    for (size_t b = 0; b < busCount; b++)
      if (power[b] != nullptr && noSensorAnswersOn(b))
        mask |= 1u << b;
    return mask;
  }

  bool powerCycleDue(unsigned long now) const
  {
    bool waitedLongEnough = !powerCycled || now - poweredAt >= ONEWIRE_POWER_CYCLE_EVERY_MS;
    return waitedLongEnough && busesToRestart() != 0;
  }

  void switchSupplies(unsigned mask, bool on)
  {
    for (size_t b = 0; b < busCount; b++)
      if (mask & (1u << b))
        power[b]->set(on);
  }

  void powerOff(unsigned long now)
  {
    restartedBuses = busesToRestart();
    switchSupplies(restartedBuses, false);
    poweredAt = now;
    powerCycled = true;
    powerCycles++;
    phase = ONEWIRE_POWERED_OFF;
  }

  void powerOn(unsigned long now)
  {
    switchSupplies(restartedBuses, true);
    poweredAt = now;
    phase = ONEWIRE_POWERING_UP;
  }

  // The readings as JSON keys ("DS18B20: Tank top": 54.3), null while a sensor does not answer
  void appendJson(char *buf, size_t size) const
  {
    for (size_t i = 0; i < count; i++)
    {
      size_t n = strlen(buf);
      if (sensors[i].valid)
        snprintf(buf + n, size - n, "\"%s\":%.2f,", sensors[i].label, sensors[i].celsius);
      else
        snprintf(buf + n, size - n, "\"%s\":null,", sensors[i].label);
    }
  }
};

#endif
