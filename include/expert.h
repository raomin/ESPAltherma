#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

// Pure request state. The adapter must hold its mutex around every access.
struct ExpertJob
{
  static const size_t TX_MAX = 256, RX_MAX = 512;
  uint32_t id = 0, startedMs = 0, elapsedMs = 0;
  uint16_t timeoutMs = 500, idleMs = 100;
  size_t txLen = 0, rxLen = 0, sent = 0;
  uint8_t tx[TX_MAX] = {}, rx[RX_MAX] = {};
  bool truncated = false, timedOut = false, cancelled = false;
};

inline int expertNibble(char c)
{
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

// Exact bytes, optionally separated by whitespace. No implicit checksum or frame repair.
inline bool expertHex(const char *s, uint8_t *out, size_t capacity, size_t &len)
{
  len = 0;
  if (!s) return false;
  int high = -1;
  for (; *s; ++s)
  {
    if (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') continue;
    int n = expertNibble(*s);
    if (n < 0) return false;
    if (high < 0) high = n;
    else
    {
      if (len == capacity) return false;
      out[len++] = (uint8_t)((high << 4) | n);
      high = -1;
    }
  }
  return high < 0 && len > 0;
}

struct ExpertSession
{
  static const uint32_t LEASE_MS = 15UL * 60UL * 1000UL;
  bool enabled = false, pending = false, running = false, done = false;
  uint32_t enabledAt = 0;
  ExpertJob job;

  bool active(uint32_t now)
  {
    if (enabled && uint32_t(now - enabledAt) >= LEASE_MS) setEnabled(false, now);
    return enabled;
  }
  void setEnabled(bool on, uint32_t now)
  {
    enabled = on;
    enabledAt = now;
    if (!on && pending)
    {
      pending = false;
      done = true;
      job.cancelled = true;
    }
  }
  bool submit(const ExpertJob &next, uint32_t now)
  {
    if (!active(now) || pending || running || next.txLen == 0 || next.txLen > ExpertJob::TX_MAX ||
        next.timeoutMs < 20 || next.timeoutMs > 2000 || next.idleMs < 5 || next.idleMs > next.timeoutMs)
      return false;
    uint32_t id = job.id + 1;
    job = next;
    job.id = id ? id : 1;
    pending = true;
    done = false;
    return true;
  }
  bool take(ExpertJob &out, uint32_t now)
  {
    if (!active(now) || !pending) return false;
    pending = false;
    running = true;
    out = job;
    return true;
  }
  void finish(const ExpertJob &result)
  {
    job = result;
    running = false;
    done = true;
  }
};
