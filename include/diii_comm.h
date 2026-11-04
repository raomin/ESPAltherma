#ifndef ESPALTHERMA_DIII_COMM_H
#define ESPALTHERMA_DIII_COMM_H

// Read-only: the DIII-NET frames that pass between the hydro board and the outdoor unit of an Altherma HT
// (EKHBRD indoor unit + ERSQ/ERRQ outdoor unit), read from the outdoor controller's RAM through its X41A port.
//
// Two frames carry values that no '@'/'I' registry of the outdoor unit gives:
//  - E0 (every 60 s, at a fixed second): from the hydro board, page 0x80 = leaving and return water (whole
//    degrees), remote controller room temperature, pump value, EVL/EVH pulses; from the outdoor unit, page 2
//    = inverter current, inverter speed, fin temperature.
//  - 63 (hydro -> outdoor, about every 5 min): return water, R410A liquid, leaving water (1/256 C) and the
//    BRC1 room thermistor.
// The frames only pass through receive/transmit slots of the RAM: the hydro E0 sits in its slot for less than
// a second, the outdoor E0 page 2 for 1-3 s, the 63 frame for 2-20 s. The reader learns the phase of each
// frame with a short burst of reads (one every 100 ms), then reads only in a window of a few seconds around
// the expected time: about 25-45 reads a minute once locked.
//
// Only the 'R' command (0x52, read 16 bytes of RAM) is ever sent; nothing is written. The addresses belong to
// one controller software: the reader first looks for its id ("DP0009") in the RAM and does nothing else on
// any other unit. Never reads 0x0000-0x03FF or 0x2C00-0x2FFF (special function registers).
//
// Verified on one unit: EKHBRD016ACV1 + ERSQ016 class outdoor, outdoor A1P software DP0009_003, ESP on the
// outdoor A1P X41A. Field meanings marked "unverified" are positions, not a confirmed physical quantity.
//
// No Arduino dependency, like control.h: the frame exchange and the clock are injected.

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "control.h" // X10aExchangeFn, x10aFrame

#define DIII_E0_PERIOD_MS 60000UL        // E0 comes once a minute, at the same second
#define DIII_E0_STALE_MS 150000UL        // an E0 value is dropped (null) after this long without a catch
#define DIII_63_STALE_MS 720000UL        // a 63 value after this long
#define DIII_63_HALFWIN_MS 15000UL       // 63 is read +-15 s around its expected time
#define DIII_CHECK_INTERVAL_MS 600000UL  // a unit that did not answer / did not match is checked again this often
#define DIII_CURRENT_INTERVAL_MS 30000UL // the RAM current candidate is read this often (when enabled)

enum DiiiStatus
{
  DIII_OFF,         // not started (start delay) or no exchange set
  DIII_NO_ANSWER,   // the 'R' command was not answered
  DIII_UNSUPPORTED, // another controller software: nothing is read
  DIII_READY
};

inline const char *diiiStatusName(DiiiStatus s)
{
  switch (s)
  {
  case DIII_NO_ANSWER:
    return "no_answer";
  case DIII_UNSUPPORTED:
    return "unsupported";
  case DIII_READY:
    return "ready";
  default:
    return "off";
  }
}

// One RAM slot an E0 frame passes through
struct DiiiTrack
{
  uint16_t addr;            // slot + 5: d[0] = frame kind, d[6] = E0[5] (page)
  unsigned long pre, post;  // window around the expected time (ms)
  unsigned long gap;        // ms between reads inside the window
  unsigned long last;       // when the frame was last seen appearing
  bool locked, done, prevE0, inWin, prime;
  uint8_t misses;
};

// --- Frame map. A row d = 16 bytes of RAM from slot+5, so d[k] = E0[k-1]; d2 = the next row (E0[0F..1E]),
// d3 = the one after (E0[1F..2E]). "used" = read into DiiiValues; everything else is not read by this code and
// is listed so it can be added. Names and conversions are Daikin's own, from the Service Diagnosis Tool model
// files (hydro: d3-in-alth "alteruma HT/LT indoor unit"; outdoor: d3-veup3-1, the VRV III RXYQ-P E0 page whose
// [1C]/[1D]/[1E] are confirmed here). Conversions: sm = sign-magnitude byte (bit 7 sign, 7-bit whole degrees),
// u = unsigned byte, 2btosad = signed whole part + next byte / 256, 2btouam = hi * 256 + lo. "-" = not in the
// SDT file. Sample = value seen on the verified unit, heating.
//
// Hydro E0 page 0x80, rx slot 0x0C85 (rows 0x0C8A, 0x0C9A, 0x0CAA). Sample 2026-10-31 18:55:11, LWT 31.
//   d[0] kind E0 | d[1..4] system id 01 00 00 00 | d[5] FF | d[6] = E0[05] page 80
//   E0     row       SDT name / conversion                              sample
//   [06]   d[7]      -                                                  D2
//   [07]   d[8]      EVL R410A expansion valve (heating), u x10 pls      C8 -> 2000      used
//   [08]   d[9]      EVH R134a expansion valve, u x10 pls                17 -> 230       used
//   [09..0A] d[10..11] Heating pump revolution speed, 2btouam rpm       0D B0 -> 3504   used
//            Pump value on the verified unit: 881 at start-up, 3524 while the compressor stops, 0 once (pump
//            off), 1300-3100 while heating, 4200 the most seen. Estimates from our data, not measured:
//            - flow ~ 5.5 L/min per 1000 (4.5-6.6), from the R134a stage energy balance at pump 1287-1762;
//              proportional over a wider range not checked. 1542 -> ~8.5 L/min; 4200 ~ the 22.9 L/min nominal
//              (16 kW at dT 10 K). A unit with a real flow sensor can check the factor.
//            - pump power ~ 5 + 96 x (n / 4200)^3 W, max 101 W (service manual; the cubic law is assumed).
//            A true rpm is not confirmed: 881 and 1542 are under the ~2500 rpm minimum of [E-03].
//   [0B]   d[12]     Compressor freq. (R134a), u Hz                     2B -> 43
//   [0C]   d[13]     bits: 7 SVBO bottom plate heater, 5 backup heater step 1, 4 backup heater step 2,
//                    2 DHW 3-way valve, 1 cooling output signal, 0 defrost solenoid valve (6, 3: commented out
//                    in the SDT file)                                   42 -> bits 6, 1
//   [0D]   d[14]     TscL R410A liquid, sm C                            09 -> 9
//   [0E]   d[15]     HTwL leaving water (heating), sm C                 1F -> 31        used
//   [0F]   d2[0]     TwR return water, sm C                             1C -> 28        used
//   [10]   d2[1]     HTh1 remote controller ambient, sm C               19 -> 25        used
//   [11]   d2[2]     TscH R134a liquid, u x2 C                          0E -> 28
//   [12]   d2[3]     HTdi discharge (R134a), u x2 C                     20 -> 64
//   [13]   d2[4]     Tfin switch box inverter fin, sm C                 1F -> 31
//   [14..15] d2[5..6] High pressure R134a, 2btosad kgf/cm2              07 21 -> 7.13
//   [16..17] d2[7..8] Low pressure R134a, 2btosad kgf/cm2               02 4C -> 2.30
//   [18]   d2[9]     -                                                  04
//   [19]   d2[10]    -                                                  08 (07..09)
//   [1A]   d2[11]    HP horse power indication code, u                  A0 -> 160 (EKHBRD016)
//   [1B]   d2[12]    -                                                  14
//   [1C]   d2[13]    -                                                  14
//   [1D]   d2[14]    Error code, u (ercd table)                         00
//   [1E]   d2[15]    -                                                  D2
//   [1F]   d3[0]     bits: 6 CH1 crankcase heater, 5 M1F switch box fan, 4 20S 4-way valve, 3/2/1/0 option
//                    I/O PCB outputs solar / DHW / error / operation    21 -> fan, operation
//   [20]   d3[1]     bits: 6 multi setpoint 2, 5 multi setpoint 1, 4 Q2L backup heater safety, 2 external room
//                    thermostat contact, 1 solar input, 0 tariff (demand) input       00
//   [21]   d3[2]     TwS domestic hot water, sm C                       AF -> -47 (no tank sensor)
//   [22]   d3[3]     TaB (commented out in the SDT file), sm C          AF -> -47
//   [23]   d3[4]     bit 5 operation ON                                 20 -> ON
//   [24]   d3[5]     -                                                  00
//   [25]   d3[6]     - (changes minute to minute: 03 04 04 05)          03
//   [26..2E]         -                                                  00
//   Page 0x81 follows within ~1 s in the same slot (short; the positions above are zero there).
//
// Outdoor E0 page 2, tx slot 0x0C25 (rows 0x0C2A, 0x0C3A, 0x0C4A). Sample 2026-10-17 17:30:55 (36 Hz):
//   d[0] kind E0 | d[1..4] system id 01 00 00 00 | d[5] 00 | d[6] = E0[05] page 02
//   [06]   d[7]      bits: 7 HP retry, 6 LP retry, 5 discharge pipe retry, 4 INV overcurrent / 3 INV fin /
//                    2 HP / 1 LP / 0 INV discharge stepping-down control 00
//   [07]   d[8]      bits: 3 INV retry, 2 demand stepping-down control (7..4: STD compressors, none here) 01
//   [08]   d[9]      bits: 7 unit error, 3 defrost, 2 oil return, 1 soft start, 0 restart stand-by   30
//   [09]   d[10]     bits: 1 compressor (INV) ON, 0 crankcase heater 3; bits 7..4 fan step           52 -> INV on, fan 5
//   [0A]   d[11]     bits: 7/6 crankcase heater 2/1, 4 20S1 4-way valve, 3 injection, 2 multi oil, 1 hot gas,
//                    0 energy cut output                                14 -> 4-way, bit 2
//   [0B]   d[12]     -                                                  80
//   [0C]   d[13]     Y1E EV opening 1, u x2 pls                         22 -> 68
//   [0D]   d[14]     EV opening (commented out), u x2 pls               00
//   [0E]   d[15]     Y2E EV opening 2, u x2 pls                         03 -> 6
//   [0F]   d2[0]     HP code, VRV table (5/6/8/10.. HP; not meaningful for ERSQ)   03
//   [10]   d2[1]     R4T coil, sm C                                     0D -> 13
//   [11]   d2[2]     R1T ambient, sm C                                  0F -> 15
//   [12]   d2[3]     R31T discharge (INV), u C                          35 -> 53
//   [13]   d2[4]     R32T discharge STD1, u C (no STD compressor)       C6
//   [14]   d2[5]     R33T discharge STD2, u C (no STD compressor)       C6
//   [15]   d2[6]     accumulator inlet, sm C                            0F -> 15
//   [16]   d2[7]     R2T suction, sm C                                  0D -> 13
//   [17]   d2[8]     receiver liquid, sm C                              16 -> 22
//   [18]   d2[9]     subcooling coil exit, sm C                         13 -> 19
//   [19]   d2[10]    -                                                  00
//   [1A]   d2[11]    TE evaporating temp, sm C (pressure: R410A table)  0D -> 13
//   [1B]   d2[12]    TC condensing temp, sm C (pressure: R410A table)   1F -> 31
//   [1C]   d2[13]    inverter revolution speed, u rps (= Hz / 2)        12 -> 18        used
//   [1D]   d2[14]    inverter (fin) temp, u C                           1E -> 30        used
//   [1E]   d2[15]    inverter current, u A (compressor/secondary)       07 -> 7         used
//   [1F]   d3[0]     CT1 (STD1), u A                                    01
//   [20]   d3[1]     CT2 (STD2), u A                                    01
//   [21]   d3[2]     INV fan current, u A                               03
//   [22..26] d3[3..7] -                                                 08 93 07 F6 0D
//   The VRV III names fit the positions checked on this ERSQ ([1C]/[1D]/[1E], R1T, discharge, Tc); the others
//   are SDT names not yet checked against a sensor. Page 1 passes 2 s earlier in the same slot.
//
// 63 (hydro -> outdoor, ~5 min), rx slot (rows 0x0C8A, 0x0C9A), SDT d3-in. Sample 2026-10-31 18:56:34.
//   d[0] kind 63 | d[1..4] 01 00 00 00 | d[5] FF; payload [n] = d[n+1]
//   [05..06] d[6..7]    return water, 8.8 BE                            1C 4C -> 28.30  used
//   [07..08] d[8..9]    R410A liquid, 8.8 BE                            04 B0 -> 4.69   used
//   [09..0A] d[10..11]  leaving water, 8.8 BE                           1F 38 -> 31.22  used
//   [0B]     d[12]      remote controller setpoint (room), u C          14 -> 20
//   [0C..0D] d[13..14]  K1E expansion valve, BE pls                     07 D0 -> 2000
//   [0E..16] d[15]..d2[7] zero
//   [17]     d2[8]      -                                               FE
//   [18..19] d2[9..10]  Air_thermistor_BRC1 room, 8.8 BE                19 E4 -> 25.89  used
//
// Other frames through the same slots (not read here; SDT d3-in / d3-veup3): 62 hydro->outdoor ([05] bits 5-4
// mode, [06] b0 run / b2 thermo ON / b4 capacity increase, [0E] error, [1A] subcode), 65 outdoor->hydro ([05] b1
// thermo / b0 restart stand-by, [09..0A] Te, [0B..0C] Tc, [1F] target Tc), 66 ([07] error, [09] subcode),
// 6A (indoor EV/fan commands), A4 (system id only), 68.

// The values; each group has the time of its last catch (0: never)
struct DiiiValues
{
  // hydro E0 page 0x80
  unsigned long hydroAt = 0;
  int lwt = 0;              // E0[0E] leaving water, whole degrees (truncated)
  unsigned pump = 0;        // E0[09..0A] pump value (unverified: rpm?)
  unsigned evl = 0, evh = 0; // E0[07], E0[08] x 10: expansion valve pulses
  unsigned long waterAt = 0;
  int rwt = 0;              // E0[0F] return water, whole degrees
  int room = 0;             // E0[10] remote controller room temperature
  // outdoor E0 page 2
  unsigned long invAt = 0;
  unsigned invRps = 0;      // E0[1C] inverter speed, rps (Hz / 2 of the 0x20 registry's frequency)
  unsigned invFin = 0;      // E0[1D] fin temperature
  unsigned invAmp = 0;      // E0[1E] inverter (secondary) current, A (not the mains current)
  // 63 frame
  unsigned long f63At = 0;
  float rwt63 = 0, liq63 = 0, lwt63 = 0, room63 = 0;
  bool room63Ok = false; // the BRC1 room value was plausible
  // RAM row 0x2B70: the outdoor unit's own current bookkeeping (not a DIII frame, always in RAM)
  unsigned long curAt = 0;
  float current = 0;   // 0x2B73 float: TOTAL outdoor input (mains) current, A
  float rated = 0;     // 0x2B79 float: rated current, A (27.0 on the verified unit)
  unsigned load = 0;   // 0x2B7D: current / rated x 256 (load ratio, truncated)
};
// 0x2B73 follows the compressor speed (r = 0.995, 2.4 A @43 Hz ... 4.4 A @71 Hz, 0.1 A steps) and fits the total
// input current computed by the sibling VRV IV / EWYQ outdoor software: sum of the inverter primary currents
// (0.1 A data) + fan currents + 0.032 A + 0.106 A per heater flag. Stopped with the crankcase heater on it reads
// 0.138 = 0.032 + 0.106 exactly. So: mains current of the outdoor unit; x 230 V x PF gives its electrical power.
// Not yet checked with a clamp meter. Compare E0[1E] (invAmp): compressor / secondary current, ~9 A at 30-39 Hz
// and 5-7 A above, NOT usable as power.
// Same row, not read: 0x2B77 word 0x0180 (1.5, a ratio limit?). Row 0x2830 bytes 0x2834/36/38 are signed small
// numbers (0..2, -1, -2), not a current.

struct DiiiComm
{
  // --- set by the caller
  X10aExchangeFn exchange = nullptr;
  unsigned long (*clock)() = nullptr;  // millis
  void (*log)(const char *) = nullptr; // optional
  void (*lock)() = nullptr;            // optional: taken while the values change (the publisher reads them)
  void (*unlock)() = nullptr;
  bool readE0 = true, read63 = true, readCurrent = true;
  bool rawLog = false;                 // log each caught frame as hex
  unsigned long startDelayMs = 90000;  // nothing is sent before this uptime (an OTA can always get in first)

  // --- state
  DiiiStatus status = DIII_OFF;
  char softwareId[11] = "";
  DiiiValues v;
  unsigned reads = 0, errors = 0;
  DiiiTrack hydro = {0x0C8A, 1500, 2000, 40, 0, false, false, false, false, true, 0};
  DiiiTrack outdoor = {0x0C2A, 1500, 2500, 100, 0, false, false, false, false, true, 0};
  unsigned long checkedAt = 0, nextRead = 0, next63 = 0, r63Arrive = 0, r63Period = 0, r63Seen = 0, curNext = 0;
  uint8_t r63Misses = 0;
  bool learnToggle = false, r63Rejected = false;

  unsigned long now() { return clock ? clock() : 0; }

  void say(const char *fmt, ...)
  {
    if (log == nullptr)
      return;
    char line[200];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);
    log(line);
  }

  static bool safeAddress(uint16_t a) { return a >= 0x0400 && a <= 0x7CF0 && !(a + 15 >= 0x2C00 && a < 0x3000); }

  // One 'R' read of 16 bytes. maxLen = the exact reply length, so the exchange returns as soon as it is complete
  // (a longer buffer would wait for 100 ms of silence: too slow for a frame that stays < 1 s in its slot).
  bool readRow(uint16_t a, uint8_t *d)
  {
    if (exchange == nullptr || !safeAddress(a))
      return false;
    uint8_t ad[2] = {(uint8_t)a, (uint8_t)(a >> 8)}, frame[8], got[18];
    size_t len = x10aFrame(0x52, ad, 2, frame);
    size_t r = exchange(frame, len, got, sizeof(got), 150);
    reads++;
    uint8_t sum = 0;
    for (size_t i = 0; i < 17 && i < r; i++)
      sum += got[i];
    if (r < 18 || got[0] != 0x52 || (uint8_t)~sum != got[17])
    {
      errors++;
      return false;
    }
    memcpy(d, got + 1, 16);
    return true;
  }

  static int btosa(uint8_t b) { return (b & 0x80) ? -(int)(b & 0x7F) : (int)b; } // sign-magnitude degrees
  static float be88(const uint8_t *p) { return (int16_t)((p[0] << 8) | p[1]) / 256.0f; }
  void begin() { if (lock) lock(); }
  void end() { if (unlock) unlock(); }

  void rawRows(const char *tag, const uint8_t *a, const uint8_t *b, const uint8_t *c)
  {
    if (!rawLog || log == nullptr)
      return;
    char line[180];
    int n = snprintf(line, sizeof(line), "%s", tag);
    const uint8_t *rows[3] = {a, b, c};
    for (const uint8_t *r : rows)
      for (int i = 0; i < 16 && n < (int)sizeof(line) - 4; i++)
        n += r ? snprintf(line + n, sizeof(line) - n, " %02X", r[i]) : snprintf(line + n, sizeof(line) - n, " ..");
    log(line);
  }

  // --- identification: the controller software id sits as text at 0x0680 ("DP0009_003" on the verified unit)
  bool identify()
  {
    checkedAt = now();
    uint8_t a[32];
    if (!readRow(0x0680, a) || !readRow(0x0690, a + 16))
    {
      status = DIII_NO_ANSWER;
      return false;
    }
    status = DIII_UNSUPPORTED;
    for (int i = 0; i + 6 <= 32; i++)
    {
      if (memcmp(a + i, "DP0009", 6) != 0)
        continue;
      int n = 0;
      while (n < 10 && i + n < 32 && a[i + n] >= 0x20 && a[i + n] < 0x7F)
        n++;
      memcpy(softwareId, a + i, n);
      softwareId[n] = 0;
      status = DIII_READY;
      break;
    }
    say("DIII reader: %s%s%s\n", diiiStatusName(status), softwareId[0] ? ", software " : "", softwareId);
    return status == DIII_READY;
  }

  void reset()
  {
    status = DIII_OFF;
    softwareId[0] = 0;
    hydro.locked = outdoor.locked = false;
    hydro.prime = outdoor.prime = true;
    r63Period = 0;
    checkedAt = 0;
  }

  // --- E0
  bool grab(DiiiTrack &t, const uint8_t *d)
  {
    if (&t == &hydro)
    {
      if (d[6] != 0x80)
        return false;
      uint8_t d2[16], d3[16];
      // E0[0F..1E] just past the first row. The frame is replaced within well under a second, so no re-read:
      // d2 is trusted only while it still holds 0x80 data (RWT, discharge/2 and fin set; the short 0x81 frame
      // and 63 leave these positions zero).
      bool ok2 = readRow(t.addr + 0x10, d2);
      bool water = ok2 && d2[0] >= 1 && d2[0] <= 90 && d2[3] != 0 && d2[4] >= 1 && d2[4] <= 90;
      unsigned long at = now();
      begin();
      v.evl = d[8] * 10;
      v.evh = d[9] * 10;
      v.pump = (d[10] << 8) | d[11];
      v.lwt = btosa(d[15]);
      v.hydroAt = at;
      if (water)
      {
        v.rwt = btosa(d2[0]);
        v.room = btosa(d2[1]);
        v.waterAt = at;
      }
      end();
      if (rawLog)
      {
        bool ok3 = readRow(t.addr + 0x20, d3);
        rawRows("E0RAW", d, ok2 ? d2 : nullptr, ok3 ? d3 : nullptr);
      }
      return true;
    }
    if (d[6] != 0x02)
      return false;
    uint8_t d2[16], d3[16];
    // both rows from the same frame: the first one unchanged on a re-read
    if (!readRow(t.addr + 0x10, d2) || !readRow(t.addr, d3) || memcmp(d, d3, 16) != 0)
      return false;
    begin();
    v.invRps = d2[0x0D];
    v.invFin = d2[0x0E];
    v.invAmp = d2[0x0F];
    v.invAt = now();
    end();
    rawRows("E0OUT", d, d2, nullptr);
    return true;
  }

  void sample(DiiiTrack &t)
  {
    uint8_t d[16];
    if (!readRow(t.addr, d))
      return;
    unsigned long at = now();
    // hydro: E0 page 0x80 or the trailing 0x81; outdoor: only its own page 2 (page 1 comes 2 s earlier)
    bool isE0 = d[0] == 0xE0 && (&t == &hydro ? d[5] == 0xFF : d[5] == 0x00 && d[6] == 0x02);
    if (t.prime)
    {
      // first read of a window or a learn run: a frame already in the slot is old, not an arrival
      t.prevE0 = isE0;
      t.prime = false;
      return;
    }
    if (isE0 && !t.prevE0)
    {
      unsigned long seen = (&t == &hydro && d[6] == 0x81) ? at - 1000 : at; // only the trailing 0x81 caught
      if (!t.locked)
      {
        long diff = (long)(seen - t.last) - (long)DIII_E0_PERIOD_MS;
        if (t.last && diff > -2500 && diff < 2500)
        {
          t.locked = true;
          say("E0 %s locked, phase %lu ms\n", &t == &hydro ? "hydro" : "outdoor", seen % DIII_E0_PERIOD_MS);
        }
      }
      t.last = seen;
      t.misses = 0;
      if (grab(t, d) && t.locked)
        t.done = true;
    }
    t.prevE0 = isE0;
  }

  // ms until a locked track's window opens (0: open now, -1: not locked)
  long untilWindow(const DiiiTrack &t)
  {
    if (!t.locked || !readE0 || status != DIII_READY)
      return -1;
    long until = (long)(t.last + DIII_E0_PERIOD_MS - t.pre - now());
    return until > 0 ? until : 0;
  }

  void serviceE0()
  {
    unsigned long t0 = now();
    if ((long)(t0 - nextRead) < 0)
      return;
    if (!hydro.locked || !outdoor.locked)
    {
      // learning: a read takes ~30 ms; each unlocked slot every 200 ms (250 ms often missed the hydro frame)
      learnToggle = !learnToggle;
      DiiiTrack &t = (learnToggle && !hydro.locked) || outdoor.locked ? hydro : outdoor;
      sample(t);
      nextRead = now() + 100;
    }
    DiiiTrack *tracks[2] = {&hydro, &outdoor};
    for (DiiiTrack *t : tracks)
    {
      if (!t->locked)
        continue;
      unsigned long open = t->last + DIII_E0_PERIOD_MS - t->pre, close = t->last + DIII_E0_PERIOD_MS + t->post;
      if ((long)(t0 - open) < 0)
      {
        t->inWin = false;
        continue;
      }
      if ((long)(t0 - close) > 0)
      {
        // window over without the frame arriving: keep the period, relearn after 3 misses
        if (++t->misses >= 3)
        {
          t->locked = false;
          t->prime = true;
          say("E0 %s lost, relearning\n", t == &hydro ? "hydro" : "outdoor");
        }
        t->last += DIII_E0_PERIOD_MS;
        t->inWin = false;
        continue;
      }
      if (!t->inWin)
      {
        t->inWin = true;
        t->done = false;
        t->prime = true;
      }
      if (!t->done)
      {
        sample(*t);
        nextRead = now() + t->gap;
      }
      return;
    }
  }

  // --- 63
  bool read63Frame()
  {
    uint8_t d[16] = {0}, d2[16], d3[16];
    // same header test as the hydro E0 (kind, d[5] FF); d[1..4] (01 00 00 00 here) may differ on other systems
    if (!readRow(0x0C8A, d) || d[0] != 0x63 || d[5] != 0xFF)
    {
      if (d[0] == 0x63)
        reject63("header", d, nullptr);
      return false;
    }
    if (!readRow(0x0C9A, d2) || !readRow(0x0C8A, d3) || memcmp(d, d3, 16) != 0)
      return false;
    float rwt = be88(d + 6), liq = be88(d + 8), lwt = be88(d + 10);
    if (!(rwt >= 1 && rwt <= 90 && lwt >= 1 && lwt <= 90))
    {
      reject63("water", d, d2);
      return false;
    }
    // the BRC1 room thermistor alone may be missing: then only its value stays null
    int16_t raw = (int16_t)((d2[9] << 8) | d2[10]);
    begin();
    v.room63Ok = raw > 0 && raw <= 45 * 256;
    v.room63 = raw / 256.0f;
    v.rwt63 = rwt;
    v.liq63 = liq;
    v.lwt63 = lwt;
    v.f63At = now();
    end();
    rawRows("63RAW", d, d2, nullptr);
    return true;
  }

  // the first 63 candidate that fails a check is logged once as hex, whatever rawLog says
  void reject63(const char *why, const uint8_t *d, const uint8_t *d2)
  {
    if (r63Rejected)
      return;
    r63Rejected = true;
    say("63 rejected (%s), please report:\n", why);
    bool raw = rawLog;
    rawLog = true;
    rawRows("63REJ", d, d2, nullptr);
    rawLog = raw;
  }

  bool nearE0Window(long ms)
  {
    long h = untilWindow(hydro), o = untilWindow(outdoor);
    return (h >= 0 && h < ms) || (o >= 0 && o < ms);
  }

  void service63()
  {
    unsigned long t0 = now();
    if ((long)(t0 - next63) < 0)
      return;
    next63 = t0 + 1000;
    if (r63Period)
    {
      unsigned long due = r63Arrive + r63Period;
      if ((long)(t0 - (due - DIII_63_HALFWIN_MS)) < 0)
        return; // window not open yet: no traffic
      if ((long)(t0 - (due + DIII_63_HALFWIN_MS)) > 0)
      {
        r63Arrive = due;
        if (++r63Misses >= 3)
        {
          r63Period = 0;
          r63Misses = 0;
          say("63 lost, relearning\n");
        }
        return;
      }
    }
    if (nearE0Window(300))
      return;
    if (!read63Frame())
      return;
    t0 = now();
    bool arrival = !r63Seen || t0 - r63Seen > 30000; // the frame stays 2-20 s: only the first catch counts
    r63Seen = t0;
    if (!arrival)
      return;
    if (r63Period)
    {
      r63Period = (3 * r63Period + (t0 - r63Arrive)) / 4; // follow a slow drift
      r63Misses = 0;
    }
    else if (r63Arrive && t0 - r63Arrive > 120000 && t0 - r63Arrive < 900000)
    {
      r63Period = t0 - r63Arrive;
      say("63 locked, period %lu s\n", r63Period / 1000);
    }
    r63Arrive = t0;
  }

  // --- RAM 0x2B73 outdoor input current, 0x2B79 rated, 0x2B7D load ratio
  void serviceCurrent()
  {
    unsigned long t0 = now();
    if ((long)(t0 - curNext) < 0 || nearE0Window(300))
      return;
    curNext = t0 + DIII_CURRENT_INTERVAL_MS;
    uint8_t d[16];
    if (!readRow(0x2B70, d))
      return;
    float f, r;
    memcpy(&f, d + 3, 4); // little-endian floats, like the ESP32
    memcpy(&r, d + 9, 4);
    if (!(f >= 0 && f < 60))
      return;
    begin();
    v.current = f;
    v.rated = (r > 0 && r < 100) ? r : 0;
    v.load = d[0x0D];
    v.curAt = now();
    end();
  }

  // --- for the caller

  // Call often from the task that owns the X10A link (every idleMs() ms at most).
  void tick()
  {
    if (exchange == nullptr || clock == nullptr || now() < startDelayMs)
      return;
    if (status != DIII_READY)
    {
      if (checkedAt == 0 || now() - checkedAt >= DIII_CHECK_INTERVAL_MS)
        identify();
      if (status != DIII_READY)
        return;
    }
    if (readE0)
      serviceE0();
    if (read63)
      service63();
    if (readCurrent)
      serviceCurrent();
  }

  // True while a phase is being learnt. The caller should then not poll on a period that divides 60 s (the
  // default 30 s does): the poll would land on the short hydro frame every minute and the phase never lock.
  bool learning() { return status == DIII_READY && readE0 && (!hydro.locked || !outdoor.locked); }

  // True when an E0 window is open or opens within ms: keep other traffic (a registry poll) off the link.
  bool windowWithin(unsigned long ms) { return nearE0Window((long)ms); }

  // How long the caller may sleep between two tick() calls
  unsigned long idleMs()
  {
    if (status != DIII_READY || !readE0)
      return 100;
    if (!hydro.locked || !outdoor.locked || nearE0Window(300))
      return 10;
    return 100;
  }

  // Waits (ticking) while an E0 window is open or opens within ms. sleep(ms) also feeds the caller's watchdog.
  void hold(unsigned long ms, void (*sleep)(unsigned long))
  {
    for (int guard = 0; guard < 2000 && windowWithin(ms); guard++)
    {
      tick();
      sleep(idleMs());
    }
  }

  // The values as ATTR keys (null when not caught recently). Call under the caller's values lock.
  void appendJson(char *buf, size_t size)
  {
    if (status != DIII_READY)
      return;
    unsigned long t = now();
    auto fresh = [t](unsigned long at, unsigned long maxAge) { return at != 0 && t - at < maxAge; };
    size_t n;
    bool h = fresh(v.hydroAt, DIII_E0_STALE_MS), w = fresh(v.waterAt, DIII_E0_STALE_MS),
         o = fresh(v.invAt, DIII_E0_STALE_MS), f = fresh(v.f63At, DIII_63_STALE_MS), c = fresh(v.curAt, 3 * DIII_CURRENT_INTERVAL_MS);
    char s[48];
#define DIII_PUT(key, ok, fmt, val)                    \
  do                                                   \
  {                                                    \
    if (ok)                                            \
      snprintf(s, sizeof(s), fmt, val);                \
    else                                               \
      strcpy(s, "null");                               \
    n = strlen(buf);                                   \
    snprintf(buf + n, size - n, "\"%s\":%s,", key, s); \
  } while (0)
    if (readE0)
    {
      DIII_PUT("E0: Leaving water (C)", h, "%d", v.lwt);
      DIII_PUT("E0: Return water (C)", w, "%d", v.rwt);
      DIII_PUT("E0: Remote room temp (C)", w, "%d", v.room);
      DIII_PUT("E0: Pump", h, "%u", v.pump);
      DIII_PUT("E0: EVL (pls)", h, "%u", v.evl);
      DIII_PUT("E0: EVH (pls)", h, "%u", v.evh);
      DIII_PUT("E0: INV current (A)", o, "%u", v.invAmp);
      DIII_PUT("E0: INV speed (rps)", o, "%u", v.invRps);
      DIII_PUT("E0: INV fin temp (C)", o, "%u", v.invFin);
      DIII_PUT("E0: Sync", true, "%d", (hydro.locked ? 1 : 0) | (outdoor.locked ? 2 : 0));
    }
    if (read63)
    {
      DIII_PUT("63: Leaving water (C)", f, "%.2f", v.lwt63);
      DIII_PUT("63: Return water (C)", f, "%.2f", v.rwt63);
      DIII_PUT("63: R410A liquid (C)", f, "%.2f", v.liq63);
      DIII_PUT("63: BRC1 room temp (C)", f && v.room63Ok, "%.2f", v.room63);
    }
    if (readCurrent)
    {
      DIII_PUT("RAM: Outdoor input current (A)", c, "%.2f", v.current);
      DIII_PUT("RAM: Outdoor rated current (A)", c && v.rated > 0, "%.1f", v.rated);
      DIII_PUT("RAM: Outdoor load (%)", c, "%u", v.load * 100 / 256);
    }
#undef DIII_PUT
  }
};

#endif
