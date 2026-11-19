#ifndef ESPALTHERMA_SGTIMEOUT_H
#define ESPALTHERMA_SGTIMEOUT_H
#include <stdint.h>
#include <stddef.h>

static constexpr uint32_t SG_TIMEOUT_MAX_SECONDS = 86400;

// No allocations, wall clock, or flash writes. Commands are remembered even while the option is disabled:
// enabling/shortening a timeout must not renew an old command merely because settings were saved.
class SgCommandTimeout
{
  uint64_t lastCommandMs = 0;
  uint32_t timeoutMs = 0;
  bool armed = false;
public:
  void configure(uint32_t seconds) { timeoutMs = seconds * 1000; }
  void accepted(uint8_t mode, uint64_t now)
  {
    lastCommandMs = now;
    armed = mode != 0;
  }
  void cancel() { armed = false; }
  uint64_t ageSeconds(uint64_t now) const { return (now - lastCommandMs) / 1000; }
  bool takeExpired(uint64_t now)
  {
    if (!armed || !timeoutMs || now - lastCommandMs < timeoutMs)
      return false;
    armed = false;
    return true;
  }
};
static_assert(sizeof(SgCommandTimeout)<=16,"SG command timeout stays small");

inline bool sgCommandMode(const uint8_t *payload, size_t length, uint8_t &mode)
{
  if (!payload)
    return false;
  auto space = [](uint8_t c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; };
  size_t first = 0, last = length;
  while (first < last && space(payload[first]))
    first++;
  while (last > first && space(payload[last - 1]))
    last--;
  if (last - first != 1 || payload[first] < '0' || payload[first] > '3')
    return false;
  mode = payload[first] - '0';
  return true;
}
#endif
