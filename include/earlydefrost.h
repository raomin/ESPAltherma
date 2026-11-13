#ifndef ESPALTHERMA_EARLYDEFROST_H
#define ESPALTHERMA_EARLYDEFROST_H

#include <stdint.h>
#include <stddef.h>

static const uint32_t DEFROST_WINDOW_MS = 15UL * 60000;
static const uint32_t DEFROST_NOTICE_MS = 24UL * 60 * 60000;
static const uint32_t DEFROST_SAMPLE_MS = 5000;
static const size_t DEFROST_SAMPLES = 181; // t=0 through t=900 s
enum DefrostFlags : uint8_t { DF_STARTUP=1, DF_DEFROST=2, DF_DHW=4, DF_COOLING=8, DF_STATUS=16, DF_DHW_KNOWN=32 };
enum IceObservation : uint8_t { ICE_NOT_CHECKED, ICE_FREE, ICE_PRESENT };
inline const char *iceObservationName(IceObservation o)
{
  return o == ICE_FREE ? "ice_free" : o == ICE_PRESENT ? "iced" : "not_checked";
}

struct DefrostInput
{
  int16_t outdoorTenths = INT16_MIN, coilTenths = INT16_MIN;
  uint8_t rps = 255, flags = 0;
  bool usable() const { return rps != 255 && (flags & DF_STATUS); }
};

// Relative timestamps have one-second resolution; temperatures are quantised to 0.5 C.
// -128 means unknown temperature, 255 means unknown rps. Each sample is exactly five bytes.
struct DefrostSample { uint8_t deltaSeconds; int8_t outdoorHalf, coilHalf; uint8_t rps, flags; };
static_assert(sizeof(DefrostSample) == 5, "compact defrost samples");

struct DefrostTrace
{
  DefrostSample samples[DEFROST_SAMPLES];
  uint32_t startMs = 0, lastMs = 0;
  uint16_t count = 0;
  bool truncated = false;

  void begin(uint32_t now) { startMs = lastMs = now; count = 0; truncated = false; }
  static int8_t half(int16_t tenths)
  {
    if (tenths == INT16_MIN) return INT8_MIN;
    int n = (tenths + (tenths >= 0 ? 2 : -2)) / 5;
    return n < -127 || n > 127 ? INT8_MIN : (int8_t)n;
  }
  void append(uint32_t now, const DefrostInput &in)
  {
    if ((uint32_t)(now-startMs) > DEFROST_WINDOW_MS || truncated) return;
    uint32_t delta = (uint32_t)(now-lastMs);
    if (count && delta < DEFROST_SAMPLE_MS-1000) return; // tolerate UART completion jitter around the 5 s cadence
    uint32_t seconds=((uint32_t)(now-startMs)+500)/1000;
    uint32_t previous=((uint32_t)(lastMs-startMs)+500)/1000;
    uint32_t deltaSeconds=seconds-previous; // round absolute times, avoiding accumulated per-interval rounding error
    if (count >= DEFROST_SAMPLES || (count && deltaSeconds > 254)) { truncated = true; return; }
    samples[count] = {(uint8_t)(count == 0 ? 0 : deltaSeconds), half(in.outdoorTenths), half(in.coilTenths), in.rps, in.flags};
    count++;
    lastMs = now;
  }
  int16_t minimumCoilTenths() const
  {
    int16_t minimum = INT16_MAX;
    for (size_t i=0;i<count;i++)
      if (samples[i].coilHalf != INT8_MIN && samples[i].coilHalf * 5 < minimum) minimum = samples[i].coilHalf * 5;
    return minimum == INT16_MAX ? INT16_MIN : minimum;
  }
};

struct EarlyDefrostEvent
{
  DefrostTrace trace;
  uint32_t id = 0, detectedMs = 0, epoch = 0;
  uint16_t elapsedSeconds = 0, startUncertaintySeconds = 0;
  int16_t outdoorTenths = INT16_MIN, coilTenths = INT16_MIN;
  IceObservation observation = ICE_NOT_CHECKED;
  bool reported = false;
};

// No heap/flash operations in this state machine. The firmware allocates it only for a verified
// supported register layout. The saved case stays intact while the next start uses the working trace.
class EarlyDefrostRecorder
{
public:
  DefrostTrace working;
  EarlyDefrostEvent saved;
private:
  uint32_t lastGoodMs = 0, lastZeroMs = 0, defrostEndedMs = 0;
  uint16_t uncertaintyMillis = 0;
  bool haveInput = false, zeroSeen = false, previousRunning = false, previousDefrost = false;
  bool knownStart = false, restartAfterDefrost = false, haveEvent = false;
public:
  bool visible(uint32_t now) const { return haveEvent && (uint32_t)(now-saved.detectedMs) < DEFROST_NOTICE_MS; }
  bool savedRecording(uint32_t now) const { return visible(now) && (uint32_t)(now-saved.trace.startMs) <= DEFROST_WINDOW_MS; }
  bool fast(uint32_t now) const
  {
    return (knownStart && (uint32_t)(now-working.startMs) <= DEFROST_WINDOW_MS) ||
           (visible(now) && (uint32_t)(now-saved.trace.startMs) <= DEFROST_WINDOW_MS);
  }
  bool observe(const DefrostInput &in, uint32_t now, uint32_t epoch)
  {
    if (haveEvent && !visible(now)) haveEvent = false;
    if (haveInput && (uint32_t)(now-lastGoodMs) > 60000)
    {
      if (visible(now) && (uint32_t)(now-saved.trace.startMs) <= DEFROST_WINDOW_MS) saved.trace.truncated = true;
      haveInput = zeroSeen = knownStart = restartAfterDefrost = false;
    }
    if (!in.usable())
    {
      if (knownStart) working.append(now,in);
      if (visible(now)) saved.trace.append(now,in);
      return false;
    }
    bool running = in.rps != 0, defrost = in.flags & DF_DEFROST, cooling = in.flags & DF_COOLING;
    if (!haveInput)
    {
      haveInput = true;
      previousRunning = running; previousDefrost = defrost;
      if (!running && !defrost) { zeroSeen = true; lastZeroMs = now; }
      lastGoodMs = now;
      return false; // booting in the middle of a run does not establish when it started
    }
    if (previousDefrost && !defrost) defrostEndedMs = now;
    if (!running && !defrost)
    {
      if (restartAfterDefrost && (uint32_t)(now-defrostEndedMs) > 60000) restartAfterDefrost = false;
      if (!restartAfterDefrost) { zeroSeen = true; lastZeroMs = now; }
    }
    if (running && !previousRunning && !defrost)
    {
      if (restartAfterDefrost) restartAfterDefrost = false;
      else if (zeroSeen && !cooling)
      {
        uint32_t uncertainty = (uint32_t)(now-lastZeroMs);
        uncertaintyMillis = uncertainty > UINT16_MAX ? UINT16_MAX : (uint16_t)uncertainty;
        working.begin(now); knownStart = true;
      }
      else if (cooling) knownStart = false;
      zeroSeen = false;
    }
    if (running && !defrost && previousDefrost) restartAfterDefrost = false;
    if (knownStart) working.append(now,in);
    bool early = defrost && !previousDefrost && !cooling && knownStart &&
                 (uint32_t)(now-working.startMs) <= DEFROST_WINDOW_MS &&
                 (uint32_t)(now-working.startMs) + uncertaintyMillis <= DEFROST_WINDOW_MS;
    if (early)
    {
      saved.trace = working;
      saved.id = saved.detectedMs = now;
      saved.epoch = epoch;
      saved.elapsedSeconds = (uint16_t)((uint32_t)(now-working.startMs)/1000);
      saved.startUncertaintySeconds = (uncertaintyMillis+999u)/1000u;
      saved.outdoorTenths=DefrostTrace::half(in.outdoorTenths)==INT8_MIN?INT16_MIN:in.outdoorTenths;
      saved.coilTenths=DefrostTrace::half(in.coilTenths)==INT8_MIN?INT16_MIN:in.coilTenths;
      saved.observation = ICE_NOT_CHECKED; saved.reported = false;
      haveEvent = true;
    }
    else if (visible(now)) saved.trace.append(now,in);
    if (defrost) restartAfterDefrost = true;
    previousRunning = running; previousDefrost = defrost; lastGoodMs = now;
    return early;
  }
  bool annotate(uint32_t id, IceObservation observation, uint32_t now)
  {
    if (!visible(now) || saved.id != id) return false;
    if (saved.observation != observation) saved.reported = false;
    saved.observation = observation;
    return true;
  }
};
static_assert(sizeof(EarlyDefrostRecorder) <= 2048, "early defrost recorder must stay within 2 KiB");

#endif
