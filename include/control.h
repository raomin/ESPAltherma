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
// RAM only (opt-in): for a target that changes often (an automation following the price or the solar surplus),
// the memory is never written. The target is held instead: a restart of the heat pump reloads the unit's curve
// from its memory, and the next read-back (every 2 minutes) writes the held target into the RAM again, as the
// power limit does. A restore releases it; the unit's memory still holds its own curve.
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
#define CONTROL_RETRY_FIRST_MS 30000UL      // a unit not ready yet (no answer, or its limits still empty just after a
                                            // power-up) is tried again this soon, then twice as long each time
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
  uint16_t ramActiveAmp;                                 // the active limit in A where the board keeps a second float (0: ramActive)
};

// The unit's lifetime counters in the hydro board's RAM (0: not mapped): the compressor's hours in each mode, the
// pump's hours and the compressor starts, all modes, 32-bit counts. The board loads them from its persistent block
// at start, each from its own slot of that block, which is how the same counter was found in every software; the
// starts are the sum of three per-mode counts in every software, the same check. 20002203 was read against its
// interface (Information > Operating hours), 18002904 on a unit.
// Then the energy of each mode: the heat produced in space heating, hot water and cooling (floats in kWh, their sum
// is the meter), the electricity used by all modes (a 32-bit count in kWh) and by space heating, hot water, cooling
// and the backup heater in space heating (32-bit counts in Wh, the same numbers in whole kWh add up to the first).
// 20002203 was read against its interface (each moved in its own mode only, the interface's energy data read the
// same). The three heat floats sit at the same persistent-block slots in every software (their sum is the meter:
// checked on 18002904 and on a 20002202 dump), and the electricity total is the store before the meter's in the
// function that fills the kWh block (13809 on an 18002904 = its Wh slots to the kWh); so those four come with the
// meter everywhere. The electricity of each mode stays on 20002203: on 18002904 the slot that is empty on 20002203
// holds the largest count (8712 of 13809 kWh), so the slots do not mean the same on every software.
struct UnitCounters
{
  uint16_t hoursHeating, hoursCooling, hoursDhw, hoursPump, starts;
  uint16_t heatHeating, heatDhw, heatCooling;
  uint16_t elecAll, elecHeating, elecDhw, elecCooling, elecBackup;
};

struct ControlProfile
{
  const char *softwareId; // software version of the hydro board, 8 digits as reported by the 'L' command
  CurveSpec dhw;          // hot water target
  CurveSpec lwt;          // leaving water target of the main zone
  LimitSpec limit;        // power limitation (missing on the boards not mapped for it)
  uint16_t ramHeatKwh;    // the unit's own energy meter, the heat produced in kWh as a 32-bit count (0: not mapped)
  UnitCounters counters;  // its operating hours and compressor starts (0: not mapped)
};

// The energy meter (the heat produced since the installation, the user interface's energy data) is a 32-bit count
// in the register area, cast from a float that is the sum of the heat of the three modes; the same code in every
// software, the two numbers equal on the units read (76F2, 18002904, 20002203, 20002202).
static const ControlProfile CONTROL_PROFILES[] = {
    // A = mild weather point, B = cold weather point. The memory bytes and mode bits are the same on both boards.
    // 16 kW EAVH/EAVX/EAVZ/EABH/EABX DA, DF and DJ series: measured on an EAVH16S18DA6V (the meter and the counters
    // read on the same unit: the meter equals the sum of the three mode floats, the starts the sum of the three
    // per-mode counts).
    {"18002904",
     {0x187A, 0x18B8, 366, 367, 0x30C0, 0x30C5, 278, 0x01, 0x1306, 0x1552},
     {0x187C, 0x18BA, 373, 372, 0x30CF, 0x30CA, 374, 0x0C, 0x136A, 0},
     {0x2FE8, 0x2FE9, 0x1880, 0x2FC8, 0x19F4, 0}, // power limitation tested live on the same unit
     0x08C2,
     {0x1A90, 0x1A74, 0x1A22, 0x1A70, 0x1B44, 0x1AA0, 0x1A36, 0x1A84, 0x08BE}},
    // 14-18 kW ETVH/ETVX/ETVZ/ETBH/ETBX 16 E/EJ and ETSH/ETSX 16 series (Altherma 3 H HT): from the software, the
    // leaving water target confirmed on an ETBH16EF9W9 (#648). identify() only accepts it when the modes and limits
    // read back make sense.
    {"20017705",
     {0x37A8, 0x37E6, 366, 367, 0x5409, 0x540E, 278, 0x01, 0x3166, 0x34C2},
     {0x37AA, 0x37E8, 373, 372, 0x5418, 0x5413, 374, 0x0C, 0x31BA, 0},
     {0x5299, 0x529A, 0x37AE, 0x5279, 0x45E0, 0x45E4},
     0x0946,
     {0x39D6, 0x39BA, 0x3980, 0x39B6, 0x3A6A, 0x39E6, 0x3994, 0x39CA, 0x0942}},
    // The profiles below come from the board's software too (same memory layout at every byte the control
    // uses, same 15 cross-checks as 20017705), none confirmed on a unit yet. Their power limitation words come
    // from the same code shape as the reference's (the kW path, the A path, the chooser and the converter all
    // found); the softwares from 20002203 on keep a second active float for the A kind.
    // 4-8 kW EHVH/EHVX/EHVZ/EHBH/EHBX DA and DJ series (Altherma 3 R)
    {"17014006",
     {0x1766, 0x17A4, 366, 367, 0x2EB5, 0x2EBA, 278, 0x01, 0x11F2, 0x143E},
     {0x1768, 0x17A6, 373, 372, 0x2EC4, 0x2EBF, 374, 0x0C, 0x1256, 0},
     {0x2DE4, 0x2DE5, 0x176C, 0x2DC4, 0x18E0, 0},
     0x08A6,
     {0x197C, 0x1960, 0x190E, 0x195C, 0x1A30, 0x198C, 0x1922, 0x1970, 0x08A2}},
    // 4-8 kW EHSH/EHSX P30/P50 D series (Altherma 3 R ECH2O)
    {"17019A04",
     {0x162A, 0x1668, 366, 367, 0x2EBE, 0x2EC3, 278, 0x01, 0x10B6, 0x1302},
     {0x162C, 0x166A, 373, 372, 0x2ECD, 0x2EC8, 374, 0x0C, 0x111A, 0},
     {0x2DF2, 0x2DF3, 0x1630, 0x2DD2, 0x17A4, 0},
     0x08A4,
     {0x1840, 0x1824, 0x17D2, 0x1820, 0x18F4, 0x1850, 0x17E6, 0x1834, 0x08A0}},
    // 9-16 kW EBLA/EDLA D monoblocs (and the 4-8 kW EHVH/EHVX/EHVZ/EHBH/EHBX E series) on the software before
    // 20002203: no image of it, the addresses come from a memory dump of an EDLA14DAW1 (Senaxx, #599) matched
    // against 20002203's layout: the converted settings and the targets sit 0x0E lower, the limit records
    // 0x65 lower, and every value read back as expected (curve 55/55 and 40/30, effective = min(ceiling, reheat),
    // reported targets equal to the registers). Not yet written to on a unit. The counters sit 6 lower than
    // 20002203's and the meter 0x0C lower: the dump holds the meter as a float (42812.0) and the same count at
    // 0x08DA, the starts (3852) as the sum of the heating and hot water starts. The limitation words 0x5B lower
    // (the kW floats 0x0E lower, with the converted settings), read by Senaxx while changing the installer
    // settings: [4-08] 0/1/2 at 0x4E15, [4-09] 0 A / 1 kW at 0x4E16, [5-05] 10 -> 2 A at 0x4DF5, [5-09] 20 -> 15 kW at
    // 0x3468. Where this software keeps the active limit is not known (nothing in 0x4180-0x41BF): not shown.
    {"20002202",
     {0x3462, 0x34A0, 366, 367, 0x4F46, 0x4F4B, 278, 0x01, 0x2EDA, 0x3152},
     {0x3464, 0x34A2, 373, 372, 0x4F55, 0x4F50, 374, 0x0C, 0x2F2E, 0},
     {0x4E15, 0x4E16, 0x3468, 0x4DF5, 0, 0},
     0x08DA,
     {0x36AC, 0x3690, 0x364A, 0x368C, 0x374C, 0x36BC, 0x365E, 0x36A0, 0x08D6}},
    // 4-8 kW EHVH/EHVX/EHVZ/EHBH/EHBX E, EA and EJ series, 9-16 kW EBLA/EDLA monoblocs and EWAA/EWYA chillers.
    // The leaving water target confirmed on two units; the meter and the counters read on an EHBX08EF6V against
    // its interface (ppuetsch, #647): the meter rises with the heat delivered, the same count again as a float at
    // 0x3756, the hours equal to the interface's.
    {"20002203",
     {0x3470, 0x34AE, 366, 367, 0x4FAB, 0x4FB0, 278, 0x01, 0x2EE8, 0x3160},
     {0x3472, 0x34B0, 373, 372, 0x4FBA, 0x4FB5, 374, 0x0C, 0x2F3C, 0},
     {0x4E70, 0x4E71, 0x3476, 0x4E50, 0x41A8, 0x41AC},
     0x08E6,
     {0x36B2, 0x3696, 0x3650, 0x3692, 0x3752, 0x36C2, 0x3664, 0x36A6, 0x08E2, 0x3722, 0x3702, 0x36FA, 0x371A}},
    // 11-16 kW EBBH/EBBX DF, EBVH/EBVX/EBVZ S18/S23 DJ and EBSH/EBSX(B) P30/P50 DF and DG series (Altherma 3 H F
    // and its ECH2O, on ERLA outdoor units)
    {"20007903",
     {0x378C, 0x37CA, 366, 367, 0x5372, 0x5377, 278, 0x01, 0x314A, 0x34A6},
     {0x378E, 0x37CC, 373, 372, 0x5381, 0x537C, 374, 0x0C, 0x319E, 0},
     {0x5207, 0x5208, 0x3792, 0x51E7, 0x4578, 0x457C},
     0x093A,
     {0x39BA, 0x399E, 0x3964, 0x399A, 0x3A4E, 0x39CA, 0x3978, 0x39AE, 0x0936}},
    // The same 11-16 kW boards on their earlier software: no image of it, the addresses come from a memory dump of an
    // EBVH16S23DJ9W (griezla, #658) matched against 20007903's layout: the converted settings and the targets sit
    // 0x12 lower (the curve 28 at 15 °C and 42 at -20 °C, the four 20.0 kW limits and the 33.08 °C leaving water
    // target all read as the unit showed them), the limit records 0x62 lower (18..50 and 18..45 for the leaving
    // water), the limitation bytes 0x56 lower, the counters 0x0A lower and the meter 0x10 lower: the meter (115715)
    // equals the sum of the mode floats, the starts (36449) the sum of the per-mode counts. Where this software
    // keeps the active limit is not known. On that unit the hours, the energy and the meter equal the interface's,
    // and the leaving water target was written both ways (held in the working memory, saved in the settings),
    // then the curve restored: 42 at -20 °C and 28 at 15 °C on the interface again.
    {"20007902",
     {0x377A, 0x37B8, 366, 367, 0x5310, 0x5315, 278, 0x01, 0x3138, 0x3494},
     {0x377C, 0x37BA, 373, 372, 0x531F, 0x531A, 374, 0x0C, 0x318C, 0},
     {0x51B1, 0x51B2, 0x3780, 0x5191, 0, 0},
     0x092A,
     {0x39B0, 0x3994, 0x395A, 0x3990, 0x3A44, 0x39C0, 0x396E, 0x39A4, 0x0926}},
    // 12 kW ETVH/ETVX/ETVZ/ETBH/ETBX EA, EJ and EF series and ETSH/ETSX 12 P30/P50 (Altherma 3 H HT 8-12 kW)
    {"20007904",
     {0x3794, 0x37D2, 366, 367, 0x53C2, 0x53C7, 278, 0x01, 0x3152, 0x34AE},
     {0x3796, 0x37D4, 373, 372, 0x53D1, 0x53CC, 374, 0x0C, 0x31A6, 0},
     {0x5253, 0x5254, 0x379A, 0x5233, 0x4580, 0x4584},
     0x0942,
     {0x39C2, 0x39A6, 0x396C, 0x39A2, 0x3A56, 0x39D2, 0x3980, 0x39B6, 0x093E}},
    // 4-8 kW EHSH/EHSX P30/P50 EF and EG series (Altherma 3 R ECH2O), 4-8 kW EBLA/EDLA E monoblocs and EWAA/EWYA
    {"20017704",
     {0x37A0, 0x37DE, 366, 367, 0x53E3, 0x53E8, 278, 0x01, 0x315E, 0x34BA},
     {0x37A2, 0x37E0, 373, 372, 0x53F2, 0x53ED, 374, 0x0C, 0x31B2, 0},
     {0x5273, 0x5274, 0x37A6, 0x5253, 0x45D8, 0x45DC},
     0x093E,
     {0x39CE, 0x39B2, 0x3978, 0x39AE, 0x3A62, 0x39DE, 0x398C, 0x39C2, 0x093A}},
    // The same ECH2O boards on their earlier software: no image of it, the addresses come from a memory dump of an
    // EHSXB08P50EF (forTT78, #642) matched against 20017704's layout: the converted settings and the targets sit
    // 0x18 lower (the curve, the limits at 20.0 kW, the reported targets and the ceiling all read as the unit
    // showed them), the limit records 0xC5 lower, the limitation bytes 0xB4 lower ([4-08] 0 and [4-09] 1 as set,
    // the four 50 A limits), the active limits at 0x3A42/0x3A46 (the board's 20.0 kW / 50.0 A defaults while no
    // limit applies). Not yet written to on a unit; no meter or counters found in the dump.
    {"20017701",
     {0x3788, 0x37C6, 366, 367, 0x531E, 0x5323, 278, 0x01, 0x3146, 0x34A2},
     {0x378A, 0x37C8, 373, 372, 0x532D, 0x5328, 374, 0x0C, 0x319A, 0},
     {0x51BF, 0x51C0, 0x378E, 0x519F, 0x3A42, 0x3A46}},
    // 12 kW ELVH/ELVX/ELVZ/ELBH/ELBX EJ/EF and ELSH/ELSX 12 P30/P50 (the 2026 series). One of the three memory-read
    // handler candidates of this software could not be resolved; the two others and the converter agree on the
    // same layout as every other board, and identify() reads everything back before a write.
    {"22009C02",
     {0x37B0, 0x37EE, 366, 367, 0x5428, 0x542D, 278, 0x01, 0x316E, 0x34CA},
     {0x37B2, 0x37F0, 373, 372, 0x5437, 0x5432, 374, 0x0C, 0x31C2, 0},
     {0x52B5, 0x52B6, 0x37B6, 0x5295, 0x45E2, 0x45E6},
     0x094A,
     {0x39DE, 0x39C2, 0x3988, 0x39BE, 0x3A72, 0x39EE, 0x399C, 0x39D2, 0x0946}},
    // 4-16 kW EHVH/EHVX/EHBH/EHBX CB series (Altherma LT of 2014-2017, AS1706432 boards). These boards have no 'L'
    // command: the software is the word of registry 0x60. Same memory bytes and mode bits as the others, the
    // four limitation converters found, a single active limit for both kinds. Read on an EHVX11S26CB9 (MvdB1982,
    // #599): the limits, both curves and the live leaving water target sit where mapped, the four kW limits at
    // 20.0. Its energy meter (the user interface's menu 6.2, heat produced) is a 32-bit count at 0x084E, the
    // same number again as a float at 0x185A.
    {"76F2",
     {0x15A4, 0x15E2, 366, 367, 0x2BF1, 0x2BF6, 278, 0x01, 0x103C, 0x1278},
     {0x15A6, 0x15E4, 373, 372, 0x2C00, 0x2BFB, 374, 0x0C, 0x10A0, 0},
     {0x2B61, 0x2B62, 0x15AA, 0x2B41, 0x1706, 0},
     0x084E,
     {0x17A2, 0x1786, 0x1734, 0x1782, 0x1856, 0x17B2, 0x1748, 0x1796, 0x084A}},
    // Two softwares mapped for the meter and the counters only (no targets yet): the 4-8 kW EHVH/EHVX/EHVZ/EHBH/EHBX
    // E series on their 2019 software, and the 2026 ground-source EGSAH/EGSAX boards.
    {"19016505", {}, {}, {}, 0x08E2, {0x37BE, 0x37A2, 0x375C, 0x379E, 0x385E, 0x37CE, 0x3770, 0x37B2, 0x08DE}},
    {"23015103", {}, {}, {}, 0x0986, {0x4070, 0x4054, 0x401A, 0x4050, 0x4104, 0x4080, 0x402E, 0x4064, 0x0982}},
    // Altherma Hybrid (AS1706433 boards, software word 5190 of registry 0x60, no 'L' command like the CB boards):
    // the meter and the counters read on an owner's unit against its interface (marcoatwork80, #647): 32520 kWh,
    // 8164 h heating, 0 cooling (no hot water by the compressor: the boiler makes it), 8713 h pump, 7899 starts
    // (= 7896 + 3 of the two per-mode counts). The hours sit 0x54 above the 76F2 profile's, the meter 0x10 above.
    {"5190", {}, {}, {}, 0x085E, {0x17F6, 0x17DA, 0x1788, 0x17D6, 0x18AA, 0, 0, 0, 0x085A}},
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
  // The hydro board's software: the 'L' command, 4 bytes, the digits reversed (04 29 00 18 is 18002904). The
  // boards of 2014-2017 (Altherma LT CA and CB) have no 'L' command and give it in registry 0x60 instead, bytes
  // 15 and 14 ("I/U Software ID (xx)" and "(yy)": 76 F2 is 76F2), the one printed by the service tools.
  bool readSoftwareId(char *out9)
  {
    uint8_t id[4];
    if (command(0x4C, nullptr, 0, id, 4))
    {
      snprintf(out9, 9, "%02X%02X%02X%02X", id[3], id[2], id[1], id[0]);
      return true;
    }
    // 03 40 60 crc -> 40 60 L payload[L-2] crc
    uint8_t reg = 0x60, frame[CONTROL_FRAME_MAX], got[CONTROL_FRAME_MAX];
    if (exchange == nullptr)
      return false;
    size_t len = x10aFrame(0x40, &reg, 1, frame);
    size_t r = exchange(frame, len, got, sizeof(got), 300);
    if (r < 4 || got[0] != 0x40 || got[1] != reg || got[2] < 18 || r < (size_t)got[2] + 2)
      return false; // no answer, or a payload too short to hold the two bytes
    uint8_t sum = 0;
    for (size_t i = 0; i < (size_t)got[2] + 1; i++)
      sum += got[i];
    if ((uint8_t)~sum != got[got[2] + 1])
      return false;
    snprintf(out9, 9, "%02X%02X", got[3 + 15], got[3 + 14]);
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
  bool ramOnly = false;             // never write the unit's memory: the target is held in its RAM (set by the caller)
  int held = -1;                    // the target held in RAM only, written again when the RAM lost it (-1: none)
  bool heldChanged = false;         // held changed: the caller persists it, so an ESP restart keeps the hold
  volatile int wanted = -1;         // requested value, -1 when none (set from other tasks)
  volatile bool restoreWanted = false; // restore the original curve (set from other tasks)
  int pendingA = -1, pendingB = -1; // the request being handled (clamped), both points
  bool restoring = false;           // the pending pair is the original curve
  bool origChanged = false;         // origA/origB changed: the caller persists them
  unsigned long wantedAt = 0, savedAt = 0, checkedAt = 0, readAt = 0;
  unsigned long retryAfter = CONTROL_RETRY_FIRST_MS; // until the next identification while not ready
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
    restoring = restoreWanted = false; // origA/origB and the hold stay: they apply again once enabled
    checkedAt = 0; // identified again as soon as enabled
    retryAfter = CONTROL_RETRY_FIRST_MS;
    changed = true;
  }

  // Reads the software version, checks the profile, the setpoint mode and the limits. True when ready.
  bool identify(unsigned long now)
  {
    checkedAt = now;
    ControlStatus before = status;
    int minBefore = minTarget, maxBefore = maxTarget;
    char softwareBefore[9];
    snprintf(softwareBefore, sizeof(softwareBefore), "%s", softwareId);
    if (!readSoftwareId(softwareId))
    {
      status = CONTROL_NO_ANSWER; // what a restart of the heat pump looks like: the hold stays
    }
    else
    {
      if (softwareBefore[0] && strcmp(softwareBefore, softwareId) != 0)
        setHeld(-1); // another hydro board software: the held target belongs to the previous one
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
        if (ramOnly && kind == CONTROL_LWT)
        {
          // The mild point's record stops at 45 by a rule of the interface for that setting (the same literal in
          // every software), while the cold point's is the unit's own range [9-01]..[9-00], and the thermostat
          // follows the curve up to [9-00] (a flat 47 gave a 50 target with a +3 K shift on an 18002904). With the
          // memory never written, the interface never sees a value it would refuse: the cold point's range applies.
          minTarget = minB;
          maxTarget = maxB;
        }
        status = minTarget >= 5 && maxTarget <= 90 && minTarget < maxTarget ? CONTROL_READY : CONTROL_UNSUPPORTED;
      }
    }
    if (status == CONTROL_NOT_CURVE_MODE)
      setHeld(-1); // the setpoint left the weather-dependent mode: coming back to it must not bring the old target back
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

  // RAM only on or off (the caller's setting): the range depends on it, so a ready control is identified again
  void setRamOnly(bool on, unsigned long now)
  {
    if (ramOnly == on)
      return;
    ramOnly = on;
    if (status == CONTROL_READY)
      identify(now);
    changed = true;
  }

  void setHeld(int value)
  {
    if (held != value)
    {
      held = value;
      heldChanged = true;
    }
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

  // The wait before the next try at a unit that is not ready: doubled each time, up to the check interval, so a
  // unit that never gets ready is not asked more often than before
  static unsigned long nextRetry(unsigned long wait)
  {
    return wait * 2 > CONTROL_CHECK_INTERVAL_MS ? CONTROL_CHECK_INTERVAL_MS : wait * 2;
  }

  // Runs the pending work: identification, requests, delayed saves, periodic reads. Owner task only.
  void tick(unsigned long now)
  {
    if (status != CONTROL_READY)
    {
      // A heat pump just powered up (with ESPAltherma, when the X10A port powers it) answers before its RAM holds
      // its limits: tried again after half a minute, then less and less often, not only at the next check
      if (checkedAt == 0 || now - checkedAt >= retryAfter)
      {
        bool retrying = checkedAt != 0;
        identify(now);
        if (status == CONTROL_READY)
          retryAfter = CONTROL_RETRY_FIRST_MS;
        else if (retrying)
          retryAfter = nextRetry(retryAfter);
      }
      return;
    }
    if (!ramOnly && held >= 0)
    {
      // RAM only switched off: the held target is saved in the unit's memory like a fresh request
      if (pendingA < 0 && wanted < 0 && !restoreWanted)
      {
        pendingA = pendingB = clamp(held); // within the range the memory allows (45 for the leaving water)
        restoring = false;
        wantedAt = now;
      }
      setHeld(-1);
    }
    // A restore takes over any request; a request cancels a restore
    if (restoreWanted)
    {
      restoreWanted = false;
      wanted = -1;
      setHeld(-1); // released: the unit's curve is back, in its RAM and (if it was changed) in its memory
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
      if (ramOnly && !restoring)
        setHeld(pendingA);
      if (savedA == pendingA && savedB == pendingB)
      {
        done = true; // already in the unit's memory
      }
      else if (ramOnly && !restoring)
      {
        done = true; // RAM only: the memory keeps the unit's curve, the target is held
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
    bool ramLostTheTarget = held >= 0 && status == CONTROL_READY && target >= 0 && (target != held || targetLo != held);
    if (ramLostTheTarget)
    {
      pendingA = pendingB = clamp(held); // a restart of the heat pump, or a save on its interface: written again next
      wantedAt = now;                    // tick, within the unit's limits (an installer may have narrowed them since)
    }
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
    if (spec->ramActive) // a board whose active limit is not mapped keeps it unknown (-1)
      readFloat(kind == LIMIT_CURRENT && spec->ramActiveAmp ? spec->ramActiveAmp : spec->ramActive, a);
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

// What the controls hold (a target kept in RAM only, the power limit) changes as often as an automation asks. The
// board keeps it for its next start, but not by saving its whole configuration in its flash at every change: once
// the value has been unchanged for a while, an hour at most after the first unsaved change, and at once when asked
// (a hold that ends must not come back after a restart). The caller keeps the latest in RTC memory meanwhile.
#define CONTROL_HOLD_SAVE_STABLE_MS 600000UL // unchanged for 10 minutes
#define CONTROL_HOLD_SAVE_MAX_MS 3600000UL   // an hour after the first unsaved change

struct DeferredSave
{
  bool pending = false, urgent = false;
  unsigned long changedAt = 0, firstAt = 0;

  void touch(unsigned long now, bool atOnce)
  {
    if (!pending)
      firstAt = now;
    pending = true;
    changedAt = now;
    urgent = urgent || atOnce;
  }

  // True once, when the save is due
  bool due(unsigned long now)
  {
    if (!pending || !(urgent || now - changedAt >= CONTROL_HOLD_SAVE_STABLE_MS || now - firstAt >= CONTROL_HOLD_SAVE_MAX_MS))
      return false;
    pending = urgent = false;
    return true;
  }
};

// The same values in the event history: a change is recorded at once when the last recorded one is older than ten
// minutes; the ones in between are counted and told in one line when the ten minutes are over. An automation moving
// a target every few minutes no longer pushes the starts, network and update events out of the history.
#define CONTROL_EVENT_EVERY_MS 600000UL

struct ChangeLog
{
  bool any = false;
  unsigned long eventAt = 0;
  unsigned skipped = 0;
  float last = 0;

  // True: record this change now
  bool changed(float value, unsigned long now)
  {
    last = value;
    if (any && now - eventAt < CONTROL_EVENT_EVERY_MS)
    {
      skipped++;
      return false;
    }
    any = true;
    eventAt = now;
    skipped = 0;
    return true;
  }

  // True once after counted changes: record where the value stands (last) and how many changes went uncounted
  bool flush(unsigned long now, unsigned &count)
  {
    if (skipped == 0 || now - eventAt < CONTROL_EVENT_EVERY_MS)
      return false;
    count = skipped;
    skipped = 0;
    eventAt = now;
    return true;
  }
};

// The unit's own energy meter: the heat produced since its installation in kWh, the number the user interface
// shows under energy metering, where the profile knows its address in the hydro board's RAM. Read-only, one
// 16-byte read a minute. Identified from the software like the targets; no profile or no address: unsupported.
#define METER_READ_INTERVAL_MS 60000UL

// The unit's lifetime counters read with the meter, where the profile maps them: lifetime totals in Home
// Assistant, in the order of UnitCounters
#define UNIT_COUNTER_COUNT 13

enum UnitCounterKind : uint8_t
{
  UNIT_HOURS,     // a lifetime count of hours
  UNIT_COUNT,     // a lifetime count
  UNIT_KWH_FLOAT, // energy, a float in kWh (kept in Wh)
  UNIT_KWH_COUNT, // energy, a 32-bit count in kWh (kept in Wh)
  UNIT_WH_COUNT,  // energy, a 32-bit count in Wh
};

struct UnitCounterDef
{
  UnitCounterKind kind;
  const char *label; // the key in MQTT and the sensor's name in Home Assistant
};

static const UnitCounterDef UNIT_COUNTER_DEFS[UNIT_COUNTER_COUNT] = {
    {UNIT_HOURS, "Unit meter: Compressor hours, space heating (h)"},
    {UNIT_HOURS, "Unit meter: Compressor hours, space cooling (h)"},
    {UNIT_HOURS, "Unit meter: Compressor hours, hot water (h)"},
    {UNIT_HOURS, "Unit meter: Pump hours (h)"},
    {UNIT_COUNT, "Unit meter: Compressor starts"},
    {UNIT_KWH_FLOAT, "Unit meter: Heat produced, space heating (kWh)"},
    {UNIT_KWH_FLOAT, "Unit meter: Heat produced, hot water (kWh)"},
    {UNIT_KWH_FLOAT, "Unit meter: Heat produced, cooling (kWh)"},
    {UNIT_KWH_COUNT, "Unit meter: Electricity used (kWh)"},
    {UNIT_WH_COUNT, "Unit meter: Electricity used, space heating (kWh)"},
    {UNIT_WH_COUNT, "Unit meter: Electricity used, hot water (kWh)"},
    {UNIT_WH_COUNT, "Unit meter: Electricity used, cooling (kWh)"},
    {UNIT_WH_COUNT, "Unit meter: Electricity used, backup heater (kWh)"},
};

inline bool unitCounterIsEnergy(UnitCounterKind k) { return k == UNIT_KWH_FLOAT || k == UNIT_KWH_COUNT || k == UNIT_WH_COUNT; }

// The value kept of a counter from its 4 bytes: hours and counts as read, energy in Wh. false: not a count (a float
// that is not a number, negative or beyond 32 bits of Wh).
inline bool unitCounterDecode(UnitCounterKind k, const uint8_t *b, uint32_t &value)
{
  uint32_t raw = (uint32_t)b[0] | (uint32_t)b[1] << 8 | (uint32_t)b[2] << 16 | (uint32_t)b[3] << 24;
  switch (k)
  {
  case UNIT_KWH_FLOAT:
  {
    float kWh;
    memcpy(&kWh, &raw, sizeof(kWh));
    if (!(kWh >= 0.0f && kWh < 4.0e6f)) // also NaN
      return false;
    value = (uint32_t)(kWh * 1000.0f + 0.5f);
    return true;
  }
  case UNIT_KWH_COUNT:
    if (raw > 4000000UL)
      return false;
    value = raw * 1000UL;
    return true;
  default:
    value = raw;
    return true;
  }
}

inline uint16_t unitCounterAddress(const UnitCounters &c, size_t i)
{
  switch (i)
  {
  case 0:
    return c.hoursHeating;
  case 1:
    return c.hoursCooling;
  case 2:
    return c.hoursDhw;
  case 3:
    return c.hoursPump;
  case 4:
    return c.starts;
  case 5:
    return c.heatHeating;
  case 6:
    return c.heatDhw;
  case 7:
    return c.heatCooling;
  case 8:
    return c.elecAll;
  case 9:
    return c.elecHeating;
  case 10:
    return c.elecDhw;
  case 11:
    return c.elecCooling;
  default:
    return c.elecBackup;
  }
}

// The unit's model name, as on its name plate (EHBX08EF6V): the first bytes of the hydro board's memory, the
// installer settings' block the targets are read from (the same 'I' command), ended by a 0. Found at the start of
// that block's image in the RAM of 20002203 (0x0968, the settings at 0x0968 + their offset); the block's layout is
// the same on every software, so the name is read there on any board, and kept only when it looks like one.
#define UNIT_MODEL_MAX 16
#define UNIT_MODEL_TRIES 3 // a unit just powered up may not have loaded it yet: tried again with the meter, then left

inline bool unitModelLooksValid(const char *name)
{
  size_t n = strlen(name);
  if (n < 4 || !(name[0] >= 'A' && name[0] <= 'Z'))
    return false;
  for (size_t i = 0; i < n; i++)
  {
    char c = name[i];
    if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '/'))
      return false;
  }
  return true;
}

// A heat pump just powered up answers before it has loaded its counts into RAM: they read 0 for a minute or so. A
// meter in use is never back at 0, so a count of 0 means "not ready": nothing is published (a lifetime total going
// to 0 and back would count twice in Home Assistant's statistics), and the unit is tried again a minute later.
struct MeterReader : X10aClient
{
  const ControlProfile *profile = nullptr;
  ControlStatus status = CONTROL_OFF;
  char softwareId[9] = "";
  char model[UNIT_MODEL_MAX + 1] = ""; // the unit's model name ("": not read, or not one)
  uint8_t modelTries = 0;
  uint32_t heatKWh = 0; // the last count read
  bool known = false;   // the last read succeeded
  unsigned long checkedAt = 0, readAt = 0;
  uint32_t counter[UNIT_COUNTER_COUNT] = {0};     // the highest count read of each mapped counter since the start (energy in Wh)
  bool counterKnown[UNIT_COUNTER_COUNT] = {false}; // the last read gave it

  void reset()
  {
    profile = nullptr;
    status = CONTROL_OFF;
    softwareId[0] = 0;
    model[0] = 0;
    modelTries = 0;
    heatKWh = 0;
    known = false;
    checkedAt = readAt = 0;
    forgetCounters();
    memset(counter, 0, sizeof(counter));
  }

  bool counterMapped(size_t i) const { return profile != nullptr && unitCounterAddress(profile->counters, i) != 0; }

  // A lifetime count read lower than one already seen (a glitch) keeps the higher one
  void readCounters()
  {
    for (size_t i = 0; i < UNIT_COUNTER_COUNT; i++)
    {
      uint8_t b[16];
      uint32_t v;
      counterKnown[i] = false;
      if (!counterMapped(i) || !readRam(unitCounterAddress(profile->counters, i), b) || !unitCounterDecode(UNIT_COUNTER_DEFS[i].kind, b, v))
        continue;
      if (v >= counter[i])
        counter[i] = v;
      counterKnown[i] = true;
    }
  }

  void forgetCounters()
  {
    for (size_t i = 0; i < UNIT_COUNTER_COUNT; i++)
      counterKnown[i] = false;
  }

  // Reads the model name until one is found, at most UNIT_MODEL_TRIES times. true: a name was found now.
  bool readModel()
  {
    if (model[0] != 0 || modelTries >= UNIT_MODEL_TRIES)
      return false;
    modelTries++;
    char name[UNIT_MODEL_MAX + 1] = {0};
    for (uint16_t i = 0; i < UNIT_MODEL_MAX; i++)
    {
      uint8_t c;
      if (!readMemory(i, c))
      {
        modelTries--; // no answer: not a try
        return false;
      }
      if (c == 0)
        break;
      name[i] = (char)c;
    }
    if (!unitModelLooksValid(name))
      return false;
    strcpy(model, name);
    return true;
  }

  bool identify(unsigned long now)
  {
    checkedAt = now;
    if (!readSoftwareId(softwareId))
    {
      status = CONTROL_NO_ANSWER;
      return false;
    }
    readModel();
    profile = nullptr;
    for (size_t i = 0; i < CONTROL_PROFILE_COUNT; i++)
      if (strcmp(CONTROL_PROFILES[i].softwareId, softwareId) == 0)
        profile = &CONTROL_PROFILES[i];
    if (profile == nullptr || profile->ramHeatKwh == 0)
    {
      status = CONTROL_UNSUPPORTED;
      return false;
    }
    status = read(now) ? CONTROL_READY : CONTROL_NO_ANSWER;
    return status == CONTROL_READY;
  }

  bool read(unsigned long now)
  {
    readAt = now;
    uint8_t b[16];
    uint32_t count = 0;
    if (profile != nullptr && readRam(profile->ramHeatKwh, b))
      count = (uint32_t)b[0] | (uint32_t)b[1] << 8 | (uint32_t)b[2] << 16 | (uint32_t)b[3] << 24;
    bool loaded = count > 0;
    bool wentBack = count < heatKWh; // heatKWh is the highest count since the start
    if (loaded && !wentBack)
      heatKWh = count;
    known = loaded;
    if (loaded)
      readCounters();
    else
      forgetCounters();
    return known;
  }

  // Heat pump task only. Identifies once, again a minute later while the unit does not answer (or has not loaded its
  // counts yet); then a read a minute.
  void tick(unsigned long now)
  {
    if (status == CONTROL_READY)
    {
      if (now - readAt >= METER_READ_INTERVAL_MS)
      {
        read(now);
        readModel();
      }
    }
    else if (status != CONTROL_UNSUPPORTED && (checkedAt == 0 || now - checkedAt >= METER_READ_INTERVAL_MS))
      identify(now);
    else if (status == CONTROL_UNSUPPORTED && model[0] == 0 && now - checkedAt >= METER_READ_INTERVAL_MS)
    {
      checkedAt = now; // no meter on this board: the name alone, a minute apart
      readModel();
    }
  }
};

#endif
