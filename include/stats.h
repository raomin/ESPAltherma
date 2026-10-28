#ifndef ESPALTHERMA_STATS_H
#define ESPALTHERMA_STATS_H

// Energy and cycle statistics, computed on the board from the values it already reads:
//  - heat output: flow x (leaving water before the backup heater - return water) x 4.18 kJ/kg.K, the heat pump's own
//    output (the backup heater adds its own after the sensor). Hot water uses the same return sensor, never the tank's.
//  - electrical input, estimated: the inverter's current x the mains voltage (a setting, 230 V by default, or the
//    unit's own reading when it has one), x sqrt(3) on a three-phase unit, plus a fixed auxiliary power (pump,
//    electronics) while the compressor runs. No power factor: an estimate, named as such.
//  - COP, estimated: the ratio of the two, while the compressor runs, outside defrosts and the first two minutes of a
//    run, smoothed over a few readings, and dropped when it makes no sense (above 11, or above what the temperatures
//    allow: Carnot x 0.55).
//  - the day's energies per mode (heating, hot water, cooling, told by the 3-way valve and the operation mode) and
//    their COP, the compressor starts (today, last hour), the last run and pause, the defrosts and the backup heater's
//    time. The day changes at the local midnight (the time zone offset is a setting the web page keeps up to date).
// The thresholds follow the ones the Daikin Cycle ML integration settled on with its users (3 rps = running, runs
// under 120 s are transients, 14 kW and COP 11 as sanity caps).
// The day's counters live in the RTC memory: they survive a restart of the board, not a power cut.
// No Arduino dependency in the logic: the native tests feed it readings and a clock.

#include <stdint.h>
#include <string.h>
#include <math.h>

#define STATS_RUN_RPS 3.0f
#define STATS_TRANSIENT_S 120
#define STATS_MAX_HEAT_KW 14.0f
#define STATS_MAX_COP 11.0f
#define STATS_CARNOT_FACTOR 0.55f
#define STATS_MIN_SAMPLES 3
#define STATS_STARTS_RING 24
#define STATS_MAGIC 0x53544132u // "STA2"

enum StatsMode : uint8_t
{
  STATS_HEATING = 0,
  STATS_DHW = 1,
  STATS_COOLING = 2
};

// One set of readings, as the page keys them. NAN: not read.
struct StatsInput
{
  float inv = NAN;     // compressor speed (rps or Hz)
  float flow = NAN;    // l/min
  float lw = NAN;      // leaving water before the backup heater (R1T)
  float inlet = NAN;   // return water (R4T)
  float amp = NAN;     // inverter primary current (A)
  float volt = NAN;    // mains voltage, when the unit reports it
  float outdoor = NAN; // outdoor air
  int v3 = -1;         // 3-way valve: 1 = hot water, 0 = space, -1 unknown
  int defrost = -1;    // 1 = defrosting
  int buh = -1;        // backup heater step 1 on
  bool cooling = false; // the operation mode says cooling
};

struct StatsSettings
{
  float voltage = 230;
  uint8_t phases = 1;
  float auxW = 0; // the configuration gives 100 W by default (config.h); 0 here keeps the tests' arithmetic plain
  int tzMinutes = 0;
};

// What survives a restart (RTC memory): the day's accumulators
struct StatsDay
{
  uint32_t magic;
  int32_t day;             // local day number (epoch / 86400), -1 while the clock is not set
  float heatKWh[3];        // per mode
  float elecKWh[3];
  uint16_t samples[3];     // readings that fed the energies, per mode
  uint16_t starts;         // compressor starts today
  uint16_t defrosts;
  uint32_t runSec;         // compressor run time today
  uint32_t buhSec;         // backup heater time today
  uint32_t startAt[STATS_STARTS_RING]; // the last starts (epoch seconds, or uptime seconds without a clock)
  uint8_t startCount;
  uint32_t lastRunSec, lastPauseSec;
};

class Stats
{
public:
  StatsSettings settings;
  StatsDay *day = nullptr; // the caller gives the RTC block

  // Live figures, NAN when not known
  float heatKW = NAN, elecKW = NAN, cop = NAN;
  bool running = false, inRun = false;
  StatsMode mode = STATS_HEATING;
  uint32_t runStartedAt = 0, pauseStartedAt = 0; // seconds (same clock as startAt)
  uint32_t lastSampleAt = 0;
  bool haveSample = false;
  float copSmoothed = NAN;
  bool lastDefrost = false;
  bool clockSeen = false;

  void begin(StatsDay *rtc)
  {
    day = rtc;
    if (day->magic != STATS_MAGIC)
    {
      memset(day, 0, sizeof(*day)); // a power-up: the RTC memory holds anything
      day->magic = STATS_MAGIC;
      day->day = -1;
    }
  }

  void resetDay(int32_t d)
  {
    uint32_t lastRun = day->lastRunSec, lastPause = day->lastPauseSec;
    memset(day, 0, sizeof(*day));
    day->magic = STATS_MAGIC;
    day->day = d;
    day->lastRunSec = lastRun;
    day->lastPauseSec = lastPause;
  }

  // Local day number from an epoch (0: clock not set)
  int32_t dayOf(uint32_t epoch) const
  {
    if (epoch == 0)
      return -1;
    return (int32_t)((epoch + settings.tzMinutes * 60L) / 86400L);
  }

  // One reading. epoch: the clock (0 when not set), uptime: seconds since boot.
  void update(const StatsInput &in, uint32_t epoch, uint32_t uptime)
  {
    uint32_t now = epoch ? epoch : uptime;
    int32_t d = dayOf(epoch);
    if (epoch && !clockSeen)
    {
      // The clock arrived (seconds after the Wi-Fi, usually): what was counted since the boot is today's, and the
      // moments kept in uptime seconds move to the epoch. Moments from before a restart are already epoch ones.
      clockSeen = true;
      uint32_t shift = epoch - uptime;
      auto rebase = [shift](uint32_t &t) { if (t && t < 1000000000u) t += shift; };
      rebase(runStartedAt);
      rebase(pauseStartedAt);
      rebase(lastSampleAt);
      for (uint8_t i = 0; i < STATS_STARTS_RING; i++)
        rebase(day->startAt[i]);
      if (day->day == -1)
        day->day = d;
    }
    if (day->day != d)
      resetDay(d);
    float dt = haveSample && now > lastSampleAt ? (float)(now - lastSampleAt) : 0;
    if (dt > 600)
      dt = 0; // a gap (a restart, a lost unit): nothing is integrated across it
    lastSampleAt = now;
    haveSample = true;

    // mode
    if (in.v3 == 1)
      mode = STATS_DHW;
    else if (in.cooling)
      mode = STATS_COOLING;
    else
      mode = STATS_HEATING;

    // compressor runs and pauses
    bool runNow = !isnan(in.inv) && in.inv >= STATS_RUN_RPS;
    if (runNow && !inRun)
    {
      inRun = true;
      runStartedAt = now;
      if (pauseStartedAt)
        day->lastPauseSec = now - pauseStartedAt;
      day->starts++;
      day->startAt[day->startCount % STATS_STARTS_RING] = now;
      day->startCount++;
    }
    else if (!runNow && inRun)
    {
      inRun = false;
      day->lastRunSec = now - runStartedAt;
      pauseStartedAt = now;
      copSmoothed = NAN;
    }
    running = runNow;
    if (runNow)
      day->runSec += (uint32_t)dt;
    if (in.buh == 1)
      day->buhSec += (uint32_t)dt;
    bool defrost = in.defrost == 1;
    if (defrost && !lastDefrost)
      day->defrosts++;
    lastDefrost = defrost;

    // powers
    heatKW = NAN;
    if (runNow && !isnan(in.flow) && in.flow > 0.5f && !isnan(in.lw) && !isnan(in.inlet))
    {
      float delta = mode == STATS_COOLING ? in.inlet - in.lw : in.lw - in.inlet;
      float kw = in.flow / 60.0f * 4.18f * delta;
      if (kw < 0)
        kw = 0;
      heatKW = kw > STATS_MAX_HEAT_KW ? NAN : kw;
    }
    elecKW = NAN;
    if (!isnan(in.amp))
    {
      float v = !isnan(in.volt) && in.volt > 100 ? in.volt : settings.voltage;
      float kw = in.amp * v * (settings.phases == 3 ? 1.7320508f : 1.0f) / 1000.0f;
      if (runNow)
        kw += settings.auxW / 1000.0f;
      elecKW = kw;
    }

    // energies of the day, while the compressor runs
    if (runNow && dt > 0)
    {
      if (!isnan(heatKW))
        day->heatKWh[mode] += heatKW * dt / 3600.0f;
      if (!isnan(elecKW))
        day->elecKWh[mode] += elecKW * dt / 3600.0f;
      if (!isnan(heatKW) && !isnan(elecKW) && day->samples[mode] < 65535)
        day->samples[mode]++;
    }

    // COP of the moment
    cop = NAN;
    bool settled = runNow && now - runStartedAt >= STATS_TRANSIENT_S && !defrost;
    if (settled && !isnan(heatKW) && !isnan(elecKW) && elecKW > 0.1f && heatKW > 0.1f)
    {
      float c = heatKW / elecKW;
      float ceiling = carnotCeiling(in.lw, in.outdoor);
      if (c <= STATS_MAX_COP && (isnan(ceiling) || c <= ceiling))
        copSmoothed = isnan(copSmoothed) ? c : copSmoothed + 0.3f * (c - copSmoothed);
    }
    if (settled && !isnan(copSmoothed))
      cop = copSmoothed;
  }

  // Carnot ceiling x 0.55, NAN when the temperatures are not known or give no lift
  static float carnotCeiling(float lw, float outdoor)
  {
    if (isnan(lw) || isnan(outdoor) || lw <= outdoor)
      return NAN;
    float th = lw + 273.15f, tc = outdoor + 273.15f;
    return th / (th - tc) * STATS_CARNOT_FACTOR;
  }

  // The day's COP of a mode, NAN until enough readings
  float copToday(StatsMode m) const
  {
    if (day->samples[m] < STATS_MIN_SAMPLES || day->elecKWh[m] < 0.05f)
      return NAN;
    float c = day->heatKWh[m] / day->elecKWh[m];
    return c > STATS_MAX_COP ? NAN : c;
  }

  // Compressor starts in the last hour
  int startsLastHour(uint32_t epoch, uint32_t uptime) const
  {
    uint32_t now = epoch ? epoch : uptime;
    int n = 0;
    uint8_t count = day->startCount < STATS_STARTS_RING ? day->startCount : STATS_STARTS_RING;
    for (uint8_t i = 0; i < count; i++)
      if (now - day->startAt[i] < 3600)
        n++;
    return n;
  }

  // Seconds of the current run or pause
  uint32_t currentSec(uint32_t epoch, uint32_t uptime) const
  {
    uint32_t now = epoch ? epoch : uptime;
    uint32_t since = inRun ? runStartedAt : pauseStartedAt;
    return since ? now - since : 0;
  }
};

#endif
