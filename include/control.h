#ifndef ESPALTHERMA_CONTROL_H
#define ESPALTHERMA_CONTROL_H

// Experimental: sets the hot water (DHW) target of the heat pump through the X10A port.
//
// The user interface owns the setpoints it shows and re-sends them every second, so they cannot be
// changed from the X10A port. But when the tank setpoint mode is "weather dependent", the hydro board
// computes the tank target itself from its curve (setpoint at a high outdoor temperature, setpoint at a
// low one). ESPAltherma writes the wanted target into both points of that curve: a flat curve, so the
// target holds whatever the weather. The curve lives in the board's RAM (applied within a second) and in
// its memory (so it survives a restart). Both are written: the RAM at once, the memory 30 s later and at
// most every 5 minutes, to spare it.
//
// The addresses are specific to a software version of the hydro board, read with the 'L' command. Only the
// versions listed in CONTROL_PROFILES are supported; nothing is ever written on another one.
//
// No Arduino dependency: the frame exchange is injected, so a unit test drives it with a fake unit.

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CONTROL_FRAME_MAX 24
#define CONTROL_SAVE_DELAY_MS 30000UL       // the target is saved in the unit's memory once unchanged for this long
#define CONTROL_SAVE_INTERVAL_MS 300000UL   // and at most this often
#define CONTROL_CHECK_INTERVAL_MS 600000UL  // the unit is identified again (mode, limits) this often
#define CONTROL_READ_INTERVAL_MS 120000UL   // the target is read back this often (the interface may change it)

// Sends a frame and collects the reply bytes. Returns the number of bytes received (0: no answer).
// firstByteMs: how long to wait for the first byte (a memory write is acknowledged once done).
typedef size_t (*X10aExchangeFn)(const uint8_t *frame, size_t len, uint8_t *reply, size_t maxLen, unsigned long firstByteMs);

struct ControlProfile
{
  const char *softwareId; // software version of the hydro board, 8 digits as reported by the 'L' command
  uint16_t ramHi, ramLo;  // RAM words (value * 256) of the curve setpoints at the high and at the low outdoor temperature
  uint16_t memHi, memLo;  // the same settings in the board's memory (6-bit value above the minimum, in bits 7..2)
  uint16_t limits;        // RAM block of the allowed ranges: records of 5 bytes (?, ?, span, minimum, step), high then low
  uint16_t modeByte;      // memory byte of the tank setpoint mode
  uint8_t modeMask;       // bit set when the tank follows the curve
};

static const ControlProfile CONTROL_PROFILES[] = {
    {"18002904", 0x187A, 0x18B8, 366, 367, 0x30C0, 278, 0x01},
};
#define CONTROL_PROFILE_COUNT (sizeof(CONTROL_PROFILES) / sizeof(CONTROL_PROFILES[0]))

enum ControlStatus
{
  CONTROL_OFF,            // disabled, or not started yet
  CONTROL_NO_ANSWER,      // the unit did not answer the identification
  CONTROL_UNSUPPORTED,    // software version without a profile
  CONTROL_NOT_CURVE_MODE, // the tank setpoint mode is not weather dependent
  CONTROL_READY
};

inline const char *controlStatusName(ControlStatus s)
{
  switch (s)
  {
  case CONTROL_NO_ANSWER:
    return "no_answer";
  case CONTROL_UNSUPPORTED:
    return "unsupported";
  case CONTROL_NOT_CURVE_MODE:
    return "not_curve_mode";
  case CONTROL_READY:
    return "ready";
  default:
    return "off";
  }
}

// Builds [len][cmd][data][checksum]; len counts itself, the command and the data. Returns the frame length.
static size_t x10aFrame(uint8_t cmd, const uint8_t *data, size_t n, uint8_t *out)
{
  out[0] = (uint8_t)(2 + n);
  out[1] = cmd;
  memcpy(out + 2, data, n);
  uint8_t sum = 0;
  for (size_t i = 0; i < 2 + n; i++)
    sum += out[i];
  out[2 + n] = (uint8_t)~sum;
  return 3 + n;
}

struct DhwControl
{
  X10aExchangeFn exchange = nullptr;
  const ControlProfile *profile = nullptr;
  ControlStatus status = CONTROL_OFF;
  char softwareId[9] = "";
  int minTarget = 0, maxTarget = 0; // allowed targets (the unit's own limits, which follow its DHW maximum)
  int target = -1;                  // curve setpoint at the high outdoor temperature, as read from the unit (-1: unknown)
  int targetLo = -1;                // curve setpoint at the low outdoor temperature (equal to target once set from here)
  int saved = -1;                   // value in the unit's memory, when both points of the curve hold the same one
  volatile int wanted = -1;         // requested value, -1 when none (set from other tasks)
  int pending = -1;                 // the request being handled (clamped)
  unsigned long wantedAt = 0, savedAt = 0, checkedAt = 0, readAt = 0;
  volatile bool changed = false; // something to publish (state or availability)
  int applied = -1;     // last value applied, for the caller's log (-1 when none since the last read)
  unsigned writes = 0;  // frames written to the unit (statistics)

  // --- protocol helpers: each returns false when the unit does not answer as expected
  bool command(uint8_t cmd, const uint8_t *data, size_t n, uint8_t *reply, size_t replyData, unsigned long firstByteMs = 300)
  {
    uint8_t frame[CONTROL_FRAME_MAX], got[CONTROL_FRAME_MAX];
    if (exchange == nullptr || n + 3 > sizeof(frame))
      return false;
    size_t len = x10aFrame(cmd, data, n, frame);
    size_t r = exchange(frame, len, got, sizeof(got), firstByteMs);
    size_t want = 2 + replyData;
    if (r < want || got[0] != cmd)
      return false;
    uint8_t sum = 0;
    for (size_t i = 0; i < want - 1; i++)
      sum += got[i];
    if ((uint8_t)~sum != got[want - 1])
      return false;
    if (reply != nullptr)
      memcpy(reply, got + 1, replyData);
    return true;
  }

  bool readRam(uint16_t addr, uint8_t *out16)
  {
    uint8_t a[2] = {(uint8_t)addr, (uint8_t)(addr >> 8)};
    return command(0x52, a, 2, out16, 16);
  }

  bool readMemory(uint16_t offset, uint8_t &out)
  {
    uint8_t a[2] = {(uint8_t)offset, (uint8_t)(offset >> 8)};
    return command(0x49, a, 2, &out, 1);
  }

  bool writeRamWord(uint16_t addr, uint16_t value)
  {
    uint8_t d[4] = {(uint8_t)addr, (uint8_t)(addr >> 8), (uint8_t)value, (uint8_t)(value >> 8)};
    writes++;
    return command(0x46, d, 4, nullptr, 0);
  }

  bool writeMemory(uint16_t offset, uint8_t value)
  {
    uint8_t d[3] = {(uint8_t)offset, (uint8_t)(offset >> 8), value};
    writes++;
    return command(0x45, d, 3, nullptr, 0, 1500);
  }

  // --- identification
  void reset()
  {
    profile = nullptr;
    status = CONTROL_OFF;
    softwareId[0] = 0;
    target = targetLo = saved = -1;
    wanted = pending = -1;
    checkedAt = 0; // identified again as soon as enabled
    changed = true;
  }

  // Reads the software version, checks the profile, the tank mode and the limits. True when ready.
  bool identify(unsigned long now)
  {
    checkedAt = now;
    ControlStatus before = status;
    int minBefore = minTarget, maxBefore = maxTarget;
    uint8_t id[4];
    if (!command(0x4C, nullptr, 0, id, 4))
    {
      status = CONTROL_NO_ANSWER;
    }
    else
    {
      snprintf(softwareId, sizeof(softwareId), "%02X%02X%02X%02X", id[3], id[2], id[1], id[0]);
      profile = nullptr;
      for (size_t i = 0; i < CONTROL_PROFILE_COUNT; i++)
      {
        if (strcmp(CONTROL_PROFILES[i].softwareId, softwareId) == 0)
          profile = &CONTROL_PROFILES[i];
      }
      uint8_t mode = 0, lim[16];
      if (profile == nullptr)
        status = CONTROL_UNSUPPORTED;
      else if (!readMemory(profile->modeByte, mode) || !readRam(profile->limits, lim))
        status = CONTROL_NO_ANSWER;
      else if (!(mode & profile->modeMask))
        status = CONTROL_NOT_CURVE_MODE;
      else
      {
        // Record: [?][?][span][minimum][step]; the values go from minimum to minimum + span - 1
        int hiMin = lim[3], hiMax = lim[3] + lim[2] - 1, loMin = lim[8], loMax = lim[8] + lim[7] - 1;
        minTarget = hiMin > loMin ? hiMin : loMin;
        maxTarget = hiMax < loMax ? hiMax : loMax;
        bool sane = lim[2] >= 1 && lim[2] <= 60 && lim[7] >= 1 && lim[7] <= 60 && minTarget >= 20 && maxTarget <= 90 && minTarget < maxTarget;
        status = sane ? CONTROL_READY : CONTROL_UNSUPPORTED;
      }
    }
    if (status == CONTROL_READY)
    {
      readTarget(now);
      readSaved();
    }
    else
      target = targetLo = saved = -1;
    if (status != before || minTarget != minBefore || maxTarget != maxBefore)
      changed = true;
    return status == CONTROL_READY;
  }

  // Reads what the unit's memory holds: the value when both points of the curve agree, -1 otherwise.
  void readSaved()
  {
    uint8_t lim[16], hi, lo;
    saved = -1;
    if (profile == nullptr || !readRam(profile->limits, lim) || !readMemory(profile->memHi, hi) || !readMemory(profile->memLo, lo))
      return;
    int vHi = (hi >> 2) + lim[3], vLo = (lo >> 2) + lim[8];
    if (vHi == vLo)
      saved = vHi;
  }

  // Reads the two curve setpoints from the unit.
  bool readTarget(unsigned long now)
  {
    readAt = now;
    uint8_t hi[16], lo[16];
    if (profile == nullptr || !readRam(profile->ramHi, hi) || !readRam(profile->ramLo, lo))
      return false;
    int t = (hi[0] | hi[1] << 8) / 256, tl = (lo[0] | lo[1] << 8) / 256;
    if (t != target || tl != targetLo)
      changed = true;
    target = t;
    targetLo = tl;
    return true;
  }

  int clamp(int value) const
  {
    return value < minTarget ? minTarget : value > maxTarget ? maxTarget : value;
  }

  // Writes the target in the RAM of the unit: applied within its next control cycle.
  bool apply(int value)
  {
    if (profile == nullptr || !writeRamWord(profile->ramHi, (uint16_t)(value << 8)) || !writeRamWord(profile->ramLo, (uint16_t)(value << 8)))
      return false;
    target = targetLo = value;
    applied = value;
    changed = true;
    return true;
  }

  // Writes the target in the memory of the unit, keeping the flag bits of each setting.
  bool persist(int value)
  {
    uint8_t lim[16], hi, lo;
    if (profile == nullptr || !readRam(profile->limits, lim) || !readMemory(profile->memHi, hi) || !readMemory(profile->memLo, lo))
      return false;
    int rawHi = value - lim[3], rawLo = value - lim[8];
    if (rawHi < 0 || rawHi > 63 || rawLo < 0 || rawLo > 63)
      return false;
    return writeMemory(profile->memHi, (uint8_t)(rawHi << 2 | (hi & 3))) && writeMemory(profile->memLo, (uint8_t)(rawLo << 2 | (lo & 3)));
  }

  // Asks for a target. Applied by tick() on the task that owns the X10A link.
  void request(int value)
  {
    wanted = value;
  }

  // Runs the pending work: identification, requests, delayed saves, periodic reads. Owner task only.
  void tick(unsigned long now)
  {
    if (status != CONTROL_READY)
    {
      if (now - checkedAt >= CONTROL_CHECK_INTERVAL_MS || checkedAt == 0)
        identify(now);
      return;
    }
    int w = wanted;
    if (w >= 0)
    {
      w = clamp(w);
      if (w != pending)
      {
        pending = w;
        wantedAt = now;
      }
      if (w != target || w != targetLo)
      {
        uint8_t mode = 0;
        if (!readMemory(profile->modeByte, mode) || !(mode & profile->modeMask) || !apply(w))
        {
          wanted = pending = -1;
          identify(now); // the unit stopped answering, or its interface left the weather-dependent mode
          return;
        }
      }
      if (saved == w)
      {
        wanted = pending = -1; // already in the unit's memory
      }
      else if (now - wantedAt >= CONTROL_SAVE_DELAY_MS && (savedAt == 0 || now - savedAt >= CONTROL_SAVE_INTERVAL_MS))
      {
        if (persist(w))
        {
          saved = w;
          savedAt = now;
          changed = true;
        }
        wanted = pending = -1; // saved, or given up for this request
      }
      return;
    }
    if (now - checkedAt >= CONTROL_CHECK_INTERVAL_MS)
      identify(now);
    else if (now - readAt >= CONTROL_READ_INTERVAL_MS)
      readTarget(now);
  }
};

#endif
