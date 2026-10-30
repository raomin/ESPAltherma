#ifndef ESPALTHERMA_CONTROL_H
#define ESPALTHERMA_CONTROL_H

// Experimental: sets the hot water target and the leaving water target of the heat pump through the X10A port.
//
// The user interface owns the setpoints it shows and re-sends them every second, so they cannot be changed
// from the X10A port. But when a setpoint mode is "weather dependent", the hydro board computes that target
// itself from its curve (a setpoint at a low outdoor temperature, one at a high outdoor temperature).
// ESPAltherma writes the wanted target into both points of the curve: a flat curve, so the target holds
// whatever the weather. The curve lives in the board's RAM (applied within a second) and in its memory (so
// it survives a restart). Both are written: the RAM at once, the memory 30 s later and at most every 5
// minutes, to spare it.
//
// The curve found before the first write is remembered (the caller keeps it across restarts): a restore request
// writes it back, both points at once, in the RAM and in the memory.
//
// What the thermostat really aims for can differ from the curve: the hot water target is capped by the
// maximum the heat pump reaches on its own at the current outdoor temperature, and the leaving water target
// carries the interface's shift and any modulation. Both are read back and reported as the effective target.
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
#define CONTROL_READ_AFTER_WRITE_MS 5000UL  // and this long after a write, once the board has recomputed

// Sends a frame and collects the reply bytes. Returns the number of bytes received (0: no answer).
// firstByteMs: how long to wait for the first byte (a memory write is acknowledged once done).
typedef size_t (*X10aExchangeFn)(const uint8_t *frame, size_t len, uint8_t *reply, size_t maxLen, unsigned long firstByteMs);

// A target the board computes from a two-point curve, and where it keeps everything about it
struct CurveSpec
{
  uint16_t ramA, ramB;     // RAM words (value * 256) of the two curve setpoints
  uint16_t memA, memB;     // the same settings in the board's memory (6-bit value above a minimum, in bits 7..2)
  uint16_t slotA, slotB;   // RAM records of their allowed ranges: 5 bytes [?, ?, span, minimum, step]
  uint16_t modeByte;       // memory byte of the setpoint mode
  uint8_t modeMask;        // bits set when the target follows the curve
  uint16_t ramEffective;   // what the thermostat really aims for (0: not known)
  uint16_t ramCeiling;     // the unit's own maximum, which caps the target (0: none)
};

// The power limitation (the installer's demand control), as the hydro board holds it in RAM: the enable byte
// (0 none, 1 continuous, 2 digital inputs), the kind byte (1 = power in kW, else current in A), limit 1 (a float
// in kW, a byte in A), the one continuous mode applies, and the active limit the board applies right now.
struct LimitSpec
{
  uint16_t ramEnable, ramKind, ramKw, ramAmp, ramActive; // all 0: not mapped on this board
};

struct ControlProfile
{
  const char *softwareId; // software version of the hydro board, 8 digits as reported by the 'L' command
  CurveSpec dhw;          // hot water target
  CurveSpec lwt;          // leaving water target of the main zone
  LimitSpec limit;        // power limitation (missing on the boards not mapped for it)
};

static const ControlProfile CONTROL_PROFILES[] = {
    // A = mild weather point, B = cold weather point. The memory bytes and mode bits are the same on both boards.
    // 16 kW EAVH/EAVX/EAVZ/EABH/EABX DA, DF and DJ series: measured on an EAVH16S18DA6V.
    {"18002904",
     {0x187A, 0x18B8, 366, 367, 0x30C0, 0x30C5, 278, 0x01, 0x1306, 0x1552},
     {0x187C, 0x18BA, 373, 372, 0x30CF, 0x30CA, 374, 0x0C, 0x136A, 0},
     {0x2FE8, 0x2FE9, 0x1880, 0x2FC8, 0x19F4}}, // power limitation tested live on the same unit
    // 14-18 kW ETVH/ETVX/ETVZ/ETBH/ETBX 16 E/EJ and ETSH/ETSX 16 series (Altherma 3 H HT): from the software, not
    // yet confirmed on a unit. identify() only accepts it when the modes and limits read back make sense.
    {"20017705",
     {0x37A8, 0x37E6, 366, 367, 0x5409, 0x540E, 278, 0x01, 0x3166, 0x34C2},
     {0x37AA, 0x37E8, 373, 372, 0x5418, 0x5413, 374, 0x0C, 0x31BA, 0}},
    // The profiles below come from the board's software too (same memory layout at every byte the control
    // uses, same 15 cross-checks as 20017705), none confirmed on a unit yet.
    // 4-8 kW EHVH/EHVX/EHVZ/EHBH/EHBX DA and DJ series (Altherma 3 R)
    {"17014006",
     {0x1766, 0x17A4, 366, 367, 0x2EB5, 0x2EBA, 278, 0x01, 0x11F2, 0x143E},
     {0x1768, 0x17A6, 373, 372, 0x2EC4, 0x2EBF, 374, 0x0C, 0x1256, 0}},
    // 4-8 kW EHSH/EHSX P30/P50 D series (Altherma 3 R ECH2O)
    {"17019A04",
     {0x162A, 0x1668, 366, 367, 0x2EBE, 0x2EC3, 278, 0x01, 0x10B6, 0x1302},
     {0x162C, 0x166A, 373, 372, 0x2ECD, 0x2EC8, 374, 0x0C, 0x111A, 0}},
    // 9-16 kW EBLA/EDLA D monoblocs (and the 4-8 kW EHVH/EHVX/EHVZ/EHBH/EHBX E series) on the software before
    // 20002203: no image of it, the addresses come from a memory dump of an EDLA14DAW1 (Senaxx, #599) matched
    // against 20002203's layout: the converted settings and the targets sit 0x0E lower, the limit records
    // 0x65 lower, and every value read back as expected (curve 55/55 and 40/30, effective = min(ceiling, reheat),
    // reported targets equal to the registers). Not yet written to on a unit.
    {"20002202",
     {0x3462, 0x34A0, 366, 367, 0x4F46, 0x4F4B, 278, 0x01, 0x2EDA, 0x3152},
     {0x3464, 0x34A2, 373, 372, 0x4F55, 0x4F50, 374, 0x0C, 0x2F2E, 0}}, // no limitation words: no image, the dump alone does not show them
    // 4-8 kW EHVH/EHVX/EHVZ/EHBH/EHBX E, EA and EJ series, 9-16 kW EBLA/EDLA monoblocs and EWAA/EWYA chillers
    {"20002203",
     {0x3470, 0x34AE, 366, 367, 0x4FAB, 0x4FB0, 278, 0x01, 0x2EE8, 0x3160},
     {0x3472, 0x34B0, 373, 372, 0x4FBA, 0x4FB5, 374, 0x0C, 0x2F3C, 0}},
    // 11-16 kW EBBH/EBBX DF, EBVH/EBVX/EBVZ S18/S23 DJ and EBSH/EBSX(B) P30/P50 DF and DG series (Altherma 3 H F
    // and its ECH2O, on ERLA outdoor units)
    {"20007903",
     {0x378C, 0x37CA, 366, 367, 0x5372, 0x5377, 278, 0x01, 0x314A, 0x34A6},
     {0x378E, 0x37CC, 373, 372, 0x5381, 0x537C, 374, 0x0C, 0x319E, 0}},
    // 12 kW ETVH/ETVX/ETVZ/ETBH/ETBX EA, EJ and EF series and ETSH/ETSX 12 P30/P50 (Altherma 3 H HT 8-12 kW)
    {"20007904",
     {0x3794, 0x37D2, 366, 367, 0x53C2, 0x53C7, 278, 0x01, 0x3152, 0x34AE},
     {0x3796, 0x37D4, 373, 372, 0x53D1, 0x53CC, 374, 0x0C, 0x31A6, 0}},
    // 4-8 kW EHSH/EHSX P30/P50 EF and EG series (Altherma 3 R ECH2O), 4-8 kW EBLA/EDLA E monoblocs and EWAA/EWYA
    {"20017704",
     {0x37A0, 0x37DE, 366, 367, 0x53E3, 0x53E8, 278, 0x01, 0x315E, 0x34BA},
     {0x37A2, 0x37E0, 373, 372, 0x53F2, 0x53ED, 374, 0x0C, 0x31B2, 0}},
    // 12 kW ELVH/ELVX/ELVZ/ELBH/ELBX EJ/EF and ELSH/ELSX 12 P30/P50 (the 2026 series). One of the three memory-read
    // handler candidates of this software could not be resolved; the two others and the converter agree on the
    // same layout as every other board, and identify() reads everything back before a write.
    {"22009C02",
     {0x37B0, 0x37EE, 366, 367, 0x5428, 0x542D, 278, 0x01, 0x316E, 0x34CA},
     {0x37B2, 0x37F0, 373, 372, 0x5437, 0x5432, 374, 0x0C, 0x31C2, 0}},
};
#define CONTROL_PROFILE_COUNT (sizeof(CONTROL_PROFILES) / sizeof(CONTROL_PROFILES[0]))

enum ControlKind
{
  CONTROL_DHW,
  CONTROL_LWT
};

enum ControlStatus
{
  CONTROL_OFF,            // disabled, or not started yet
  CONTROL_NO_ANSWER,      // the unit did not answer the identification
  CONTROL_UNSUPPORTED,    // software version without a profile for this target
  CONTROL_NOT_CURVE_MODE, // the setpoint mode is not weather dependent
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

// The X10A link shared by the controls: frames, RAM and memory reads and writes
struct X10aClient
{
  X10aExchangeFn exchange = nullptr;
  unsigned writes = 0; // frames written to the unit (statistics)

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

  // A word (value * 256) of the RAM, as a whole number of degrees; false when the unit does not answer
  bool readWord(uint16_t addr, int &out)
  {
    uint8_t b[16];
    if (!readRam(addr, b))
      return false;
    int w = b[0] | b[1] << 8;
    out = (w >= 0x8000 ? w - 0x10000 : w) / 256;
    return true;
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

  bool writeRamBytes(uint16_t addr, const uint8_t *bytes, size_t n)
  {
    uint8_t d[6] = {(uint8_t)addr, (uint8_t)(addr >> 8)};
    if (n < 1 || n > 4)
      return false;
    memcpy(d + 2, bytes, n);
    writes++;
    return command(0x46, d, 2 + n, nullptr, 0);
  }

  bool readFloat(uint16_t addr, float &out)
  {
    uint8_t b[16];
    if (!readRam(addr, b))
      return false;
    memcpy(&out, b, 4); // little-endian IEEE 754, like the ESP32
    return true;
  }

  bool readByte(uint16_t addr, uint8_t &out)
  {
    uint8_t b[16];
    if (!readRam(addr, b))
      return false;
    out = b[0];
    return true;
  }

  // The software version of the hydro board, 8 hex digits, as the 'L' command reports it
  bool readSoftwareId(char *out9)
  {
    uint8_t id[4];
    if (!command(0x4C, nullptr, 0, id, 4))
      return false;
    snprintf(out9, 9, "%02X%02X%02X%02X", id[3], id[2], id[1], id[0]);
    return true;
  }
};

struct CurveControl : X10aClient
{
  ControlKind kind = CONTROL_DHW;
  const ControlProfile *profile = nullptr;
  const CurveSpec *spec = nullptr;
  ControlStatus status = CONTROL_OFF;
  char softwareId[9] = "";
  int minTarget = 0, maxTarget = 0; // allowed targets (the unit's own limits)
  int minA = 0, minB = 0;           // minimum of each curve setting, the base of its memory encoding
  int target = -1;                  // curve setpoint at the high outdoor temperature, as read from the unit (-1: unknown)
  int targetLo = -1;                // curve setpoint at the low outdoor temperature (equal to target once set from here)
  int effective = -1;               // what the thermostat really aims for (-1: unknown)
  int ceiling = -1;                 // the unit's own maximum (-1: none or unknown)
  int saved = -1;                   // value in the unit's memory, when both points of the curve hold the same one
  int savedA = -1, savedB = -1;     // each point as the unit's memory holds it
  int origA = -1, origB = -1;       // the curve found before the first write from here (-1: none), restorable
  volatile int wanted = -1;         // requested value, -1 when none (set from other tasks)
  volatile bool restoreWanted = false; // restore the original curve (set from other tasks)
  int pendingA = -1, pendingB = -1; // the request being handled (clamped), both points
  bool restoring = false;           // the pending pair is the original curve
  bool origChanged = false;         // origA/origB changed: the caller persists them
  unsigned long wantedAt = 0, savedAt = 0, checkedAt = 0, readAt = 0;
  volatile bool changed = false; // something to publish (state or availability)
  int applied = -1, appliedLo = -1; // last pair applied, for the caller's log (-1 when none since the last read)

  // Allowed range of a curve setting from its limit record: [minimum, minimum + span - 1]
  bool readSlot(uint16_t slot, int &min, int &max)
  {
    uint8_t b[16];
    if (!readRam(slot, b))
      return false;
    min = b[3];
    max = b[3] + b[2] - 1;
    return b[2] >= 1 && b[2] <= 60 && min >= 0 && min <= 90;
  }

  // --- identification
  void reset()
  {
    profile = nullptr;
    spec = nullptr;
    status = CONTROL_OFF;
    softwareId[0] = 0;
    target = targetLo = saved = savedA = savedB = effective = ceiling = -1;
    wanted = pendingA = pendingB = -1;
    restoring = restoreWanted = false; // origA/origB stay: the curve can still be restored once enabled again
    checkedAt = 0; // identified again as soon as enabled
    changed = true;
  }

  // Reads the software version, checks the profile, the setpoint mode and the limits. True when ready.
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
      spec = profile == nullptr ? nullptr : kind == CONTROL_LWT ? &profile->lwt : &profile->dhw;
      uint8_t mode = 0;
      int maxA = 0, maxB = 0;
      if (spec == nullptr || spec->ramA == 0)
        status = CONTROL_UNSUPPORTED;
      else if (!readMemory(spec->modeByte, mode))
        status = CONTROL_NO_ANSWER;
      else if (!(mode & spec->modeMask))
        status = CONTROL_NOT_CURVE_MODE;
      else if (!readSlot(spec->slotA, minA, maxA) || !readSlot(spec->slotB, minB, maxB))
        status = CONTROL_UNSUPPORTED; // no answer, or limits that make no sense: not ours to write
      else
      {
        minTarget = minA > minB ? minA : minB;
        maxTarget = maxA < maxB ? maxA : maxB;
        status = minTarget >= 5 && maxTarget <= 90 && minTarget < maxTarget ? CONTROL_READY : CONTROL_UNSUPPORTED;
      }
    }
    if (status == CONTROL_READY)
    {
      readTarget(now);
      readSaved();
    }
    else
      target = targetLo = saved = effective = ceiling = -1;
    if (status != before || minTarget != minBefore || maxTarget != maxBefore)
      changed = true;
    return status == CONTROL_READY;
  }

  // Reads what the unit's memory holds: the value when both points of the curve agree, -1 otherwise.
  void readSaved()
  {
    uint8_t a, b;
    saved = savedA = savedB = -1;
    if (spec == nullptr || !readMemory(spec->memA, a) || !readMemory(spec->memB, b))
      return;
    savedA = (a >> 2) + minA;
    savedB = (b >> 2) + minB;
    if (savedA == savedB)
      saved = savedA;
  }

  // Reads the two curve setpoints, the effective target and the ceiling from the unit.
  bool readTarget(unsigned long now)
  {
    readAt = now;
    int a, b, e = -1, c = -1;
    if (spec == nullptr || !readWord(spec->ramA, a) || !readWord(spec->ramB, b))
      return false;
    if (spec->ramEffective)
      readWord(spec->ramEffective, e);
    if (spec->ramCeiling)
      readWord(spec->ramCeiling, c);
    if (a != target || b != targetLo || e != effective || c != ceiling)
      changed = true;
    target = a;
    targetLo = b;
    effective = e;
    ceiling = c;
    return true;
  }

  int clamp(int value) const
  {
    return value < minTarget ? minTarget : value > maxTarget ? maxTarget : value;
  }

  // Writes the two points of the curve in the RAM of the unit: applied within its next control cycle.
  bool apply(int a, int b, unsigned long now)
  {
    if (spec == nullptr || !writeRamWord(spec->ramA, (uint16_t)(a << 8)) || !writeRamWord(spec->ramB, (uint16_t)(b << 8)))
      return false;
    target = a;
    targetLo = b;
    applied = a;
    appliedLo = b;
    readAt = now + CONTROL_READ_AFTER_WRITE_MS - CONTROL_READ_INTERVAL_MS; // read back once the board recomputed
    changed = true;
    return true;
  }

  // Writes the two points of the curve in the memory of the unit, keeping the flag bits of each setting.
  bool persist(int vA, int vB)
  {
    uint8_t a, b;
    if (spec == nullptr || !readMemory(spec->memA, a) || !readMemory(spec->memB, b))
      return false;
    int rawA = vA - minA, rawB = vB - minB;
    if (rawA < 0 || rawA > 63 || rawB < 0 || rawB > 63)
      return false;
    return writeMemory(spec->memA, (uint8_t)(rawA << 2 | (a & 3))) && writeMemory(spec->memB, (uint8_t)(rawB << 2 | (b & 3)));
  }

  // Asks for a target. Applied by tick() on the task that owns the X10A link.
  void request(int value)
  {
    wanted = value;
  }

  // Asks for the curve found before the first write to be put back. Nothing happens when none is known.
  void requestRestore()
  {
    restoreWanted = true;
  }

  // The curve known from before the first write, kept by the caller across restarts (-1: none)
  void setOriginal(int a, int b)
  {
    origA = a;
    origB = b;
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
    // A restore takes over any request; a request cancels a restore
    if (restoreWanted)
    {
      restoreWanted = false;
      wanted = -1;
      if (origA >= 0 && origB >= 0 && !restoring)
      {
        pendingA = origA;
        pendingB = origB;
        restoring = true;
        wantedAt = now;
        changed = true;
      }
    }
    int w = wanted;
    if (w >= 0)
    {
      w = clamp(w);
      if (w != pendingA || w != pendingB || restoring)
      {
        pendingA = pendingB = w;
        restoring = false;
        wantedAt = now;
      }
    }
    if (pendingA >= 0)
    {
      if (pendingA != target || pendingB != targetLo)
      {
        uint8_t mode = 0;
        int beforeA = target, beforeB = targetLo;
        if (!readMemory(spec->modeByte, mode) || !(mode & spec->modeMask) || !apply(pendingA, pendingB, now))
        {
          wanted = pendingA = pendingB = -1;
          restoring = false;
          identify(now); // the unit stopped answering, or its interface left the weather-dependent mode
          return;
        }
        if (!restoring && origA < 0 && beforeA >= 0 && beforeB >= 0)
        {
          origA = beforeA; // the unit's own curve, from before the first write
          origB = beforeB;
          origChanged = true;
        }
      }
      bool done = false;
      if (savedA == pendingA && savedB == pendingB)
      {
        done = true; // already in the unit's memory
      }
      else if ((restoring || now - wantedAt >= CONTROL_SAVE_DELAY_MS) && (savedAt == 0 || now - savedAt >= CONTROL_SAVE_INTERVAL_MS))
      {
        if (persist(pendingA, pendingB))
        {
          savedA = pendingA;
          savedB = pendingB;
          saved = savedA == savedB ? savedA : -1;
          savedAt = now;
          changed = true;
        }
        done = true; // saved, or given up for this request
      }
      if (done)
      {
        if (restoring)
        {
          origA = origB = -1; // the unit has its curve back
          origChanged = true;
          restoring = false;
          changed = true;
        }
        wanted = pendingA = pendingB = -1;
      }
      if (now - readAt >= CONTROL_READ_INTERVAL_MS)
        readTarget(now);
      return;
    }
    if (now - checkedAt >= CONTROL_CHECK_INTERVAL_MS)
      identify(now);
    else if (now - readAt >= CONTROL_READ_INTERVAL_MS)
      readTarget(now);
  }
};

// Caps the heat pump's electrical power (or current): the installer's "power limitation" field settings, written
// in the hydro board's RAM: limit 1 and the enable byte set to "continuous". The unit applies it within a cycle
// and reports what it applies in the active limit. Below the compressor's minimum the unit stops it, hence a
// floor on the value. Nothing is written in the board's memory: a restart of the heat pump brings its own
// settings back, and this control re-applies the wanted limit as soon as it sees it gone (the caller keeps the
// wanted limit in its configuration). Switching off restores the enable byte found before the first write.
#define LIMIT_REAPPLY_MS 60000UL  // a lost limit is written again at most this often
#define LIMIT_READ_INTERVAL_MS 120000UL
#define LIMIT_MIN_KW 1.0f
#define LIMIT_MAX_KW 20.0f
#define LIMIT_MIN_AMP 4.0f
#define LIMIT_MAX_AMP 50.0f

enum LimitKind
{
  LIMIT_POWER,  // the unit limits power, kW
  LIMIT_CURRENT // the unit limits current, A
};

struct LimitControl : X10aClient
{
  const ControlProfile *profile = nullptr;
  const LimitSpec *spec = nullptr;
  ControlStatus status = CONTROL_OFF;
  char softwareId[9] = "";
  LimitKind kind = LIMIT_POWER;
  float minLimit = LIMIT_MIN_KW, maxLimit = LIMIT_MAX_KW;
  int enable = -1;            // the unit's enable byte as read (-1: unknown)
  int origEnable = -1;        // the enable byte found before the first write from here (-1: none)
  float limit = -1;           // limit 1 as the unit holds it (-1: unknown)
  float active = -1;          // the limit the unit applies right now (-1: unknown)
  bool on = false;            // a limit from here is applied
  float value = -1;           // the limit applied from here (-1: none)
  volatile float wanted = -1; // requested limit, -1 when none (set from other tasks)
  volatile int wantedOn = -1; // 1 to apply the current value, 0 to switch off, -1 none (set from other tasks)
  volatile bool changed = false;
  bool stateChanged = false;  // on/value/origEnable changed: the caller persists them
  float applied = -1;         // last limit written, for the caller's log (-1 when none since the last read)
  bool released = false;      // switched off since the last read, for the caller's log
  unsigned long checkedAt = 0, readAt = 0, wroteAt = 0;

  void reset()
  {
    profile = nullptr;
    spec = nullptr;
    status = CONTROL_OFF;
    softwareId[0] = 0;
    enable = -1;
    limit = active = -1;
    wanted = -1;
    wantedOn = -1;
    checkedAt = 0;
    changed = true;
  }

  // Reads the software version, checks the profile and reads the limitation state. True when ready.
  bool identify(unsigned long now)
  {
    checkedAt = now;
    ControlStatus before = status;
    if (!readSoftwareId(softwareId))
    {
      status = CONTROL_NO_ANSWER;
    }
    else
    {
      profile = nullptr;
      for (size_t i = 0; i < CONTROL_PROFILE_COUNT; i++)
        if (strcmp(CONTROL_PROFILES[i].softwareId, softwareId) == 0)
          profile = &CONTROL_PROFILES[i];
      spec = profile == nullptr || profile->limit.ramEnable == 0 ? nullptr : &profile->limit;
      uint8_t k = 0;
      if (spec == nullptr)
        status = CONTROL_UNSUPPORTED;
      else if (!readByte(spec->ramKind, k))
        status = CONTROL_NO_ANSWER;
      else
      {
        kind = k == 1 ? LIMIT_POWER : LIMIT_CURRENT;
        minLimit = kind == LIMIT_POWER ? LIMIT_MIN_KW : LIMIT_MIN_AMP;
        maxLimit = kind == LIMIT_POWER ? LIMIT_MAX_KW : LIMIT_MAX_AMP;
        status = readBack(now) ? CONTROL_READY : CONTROL_NO_ANSWER;
      }
    }
    if (status != CONTROL_READY)
    {
      enable = -1;
      limit = active = -1;
    }
    if (status != before)
      changed = true;
    return status == CONTROL_READY;
  }

  // Reads the enable byte, limit 1 and the active limit from the unit.
  bool readBack(unsigned long now)
  {
    readAt = now;
    uint8_t e = 0;
    float l = -1, a = -1;
    if (spec == nullptr || !readByte(spec->ramEnable, e))
      return false;
    if (kind == LIMIT_POWER)
    {
      if (!readFloat(spec->ramKw, l))
        return false;
    }
    else
    {
      uint8_t b = 0;
      if (!readByte(spec->ramAmp, b))
        return false;
      l = b;
    }
    readFloat(spec->ramActive, a);
    if (e != enable || l != limit || a != active)
      changed = true;
    enable = e;
    limit = l;
    active = a;
    return true;
  }

  float clamp(float v) const
  {
    return v < minLimit ? minLimit : v > maxLimit ? maxLimit : v;
  }

  // Writes limit 1 and, when needed, the enable byte. The enable byte found first is remembered.
  bool apply(float v, unsigned long now)
  {
    if (spec == nullptr || enable < 0)
      return false;
    bool ok;
    if (kind == LIMIT_POWER)
    {
      uint8_t f[4];
      memcpy(f, &v, 4);
      ok = writeRamBytes(spec->ramKw, f, 4);
    }
    else
    {
      uint8_t b = (uint8_t)(v + 0.5f);
      ok = writeRamBytes(spec->ramAmp, &b, 1);
    }
    if (!ok)
      return false;
    if (enable != 1)
    {
      if (!on && origEnable < 0)
        origEnable = enable; // the installer's own setting, put back when switched off
      uint8_t one = 1;
      if (!writeRamBytes(spec->ramEnable, &one, 1))
        return false;
      enable = 1;
    }
    limit = v;
    value = v;
    on = true;
    applied = v;
    wroteAt = now;
    readAt = now + 5000 - LIMIT_READ_INTERVAL_MS; // read back once the board has recomputed
    stateChanged = changed = true;
    return true;
  }

  // Puts the enable byte back as found (or to "none") and forgets the limit.
  bool release(unsigned long now)
  {
    if (spec == nullptr)
      return false;
    uint8_t e = (uint8_t)(origEnable >= 0 && origEnable != 1 ? origEnable : 0);
    if (!writeRamBytes(spec->ramEnable, &e, 1))
      return false;
    enable = e;
    on = false;
    value = -1;
    origEnable = -1;
    released = true;
    wroteAt = now;
    readAt = now + 5000 - LIMIT_READ_INTERVAL_MS;
    stateChanged = changed = true;
    return true;
  }

  void request(float v) { wanted = v; }
  void requestOn(bool wantOn) { wantedOn = wantOn ? 1 : 0; }

  // The limit the caller saved (from before a restart of ESPAltherma): applied once the unit is ready
  void restore(bool savedOn, float savedValue, int savedOrigEnable)
  {
    if (!savedOn || savedValue < 0)
      return;
    value = savedValue;
    origEnable = savedOrigEnable;
    wantedOn = 1;
  }

  // Runs the pending work: identification, requests, re-application, periodic reads. Owner task only.
  void tick(unsigned long now)
  {
    if (status != CONTROL_READY)
    {
      if (now - checkedAt >= CONTROL_CHECK_INTERVAL_MS || checkedAt == 0)
        identify(now);
      return;
    }
    int wOn = wantedOn;
    float w = wanted;
    if (wOn == 0)
    {
      wantedOn = -1;
      wanted = -1;
      if (on && !release(now))
        identify(now);
      return;
    }
    if (w >= 0 || wOn == 1)
    {
      wanted = -1;
      wantedOn = -1;
      float v = clamp(w >= 0 ? w : value);
      if (v >= minLimit && !apply(v, now))
        identify(now);
      return;
    }
    if (now - readAt >= LIMIT_READ_INTERVAL_MS)
    {
      if (!readBack(now))
      {
        identify(now);
        return;
      }
      // The heat pump restarted (its settings came back) or someone changed them: apply ours again
      if (on && value >= 0 && (enable != 1 || limit < value - 0.05f || limit > value + 0.05f) && now - wroteAt >= LIMIT_REAPPLY_MS)
      {
        int keep = origEnable;
        apply(value, now);
        origEnable = keep;
      }
    }
    else if (now - checkedAt >= CONTROL_CHECK_INTERVAL_MS)
      identify(now);
  }
};

#endif
