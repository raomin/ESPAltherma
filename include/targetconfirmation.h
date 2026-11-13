#ifndef ESPALTHERMA_TARGETCONFIRMATION_H
#define ESPALTHERMA_TARGETCONFIRMATION_H

#include <stdint.h>

enum TargetState : uint8_t { TARGET_OBSERVED, TARGET_QUEUED, TARGET_WRITTEN, TARGET_CONFIRMED,
                            TARGET_DIFFERENT, TARGET_READ_FAILED, TARGET_WRITE_FAILED };
struct TargetConfirmation
{
  int requested = -1, expectedA = -1, expectedB = -1;
  uint32_t lastCompleteRead = 0;
  uint8_t flags = 0;
  TargetState state = TARGET_OBSERVED;
  enum { HAS_READ = 1, PENDING = 2, EXPECT = 4, WRITE_FAILED = 8 };

  void queue(int value) { requested = value; flags = (flags & HAS_READ) | PENDING; state = TARGET_QUEUED; }
  void expect(int a, int b)
  {
    expectedA = a; expectedB = b;
    flags = (flags & HAS_READ) | EXPECT;
    state = TARGET_QUEUED;
  }
  void written(int a, int b)
  {
    expect(a, b);
    state = TARGET_WRITTEN;
  }
  void writeFailed() { flags |= WRITE_FAILED; state = TARGET_WRITE_FAILED; }
  void readFailed() { if (!(flags & WRITE_FAILED)) state = TARGET_READ_FAILED; }
  void observe(int a, int b, bool complete, uint32_t now)
  {
    if (!complete) { readFailed(); return; }
    flags |= HAS_READ;
    lastCompleteRead = now;
    if (flags & WRITE_FAILED) return;
    if (flags & PENDING) state = TARGET_QUEUED;
    else if (flags & EXPECT) state = a == expectedA && b == expectedB ? TARGET_CONFIRMED : TARGET_DIFFERENT;
    else state = TARGET_OBSERVED;
  }
  int32_t ageSeconds(uint32_t now) const
  {
    return flags & HAS_READ ? (int32_t)((uint32_t)(now - lastCompleteRead) / 1000) : -1;
  }
  bool fresh(uint32_t now, uint32_t maxAge) const
  {
    return (flags & HAS_READ) && (uint32_t)(now - lastCompleteRead) < maxAge &&
           (state == TARGET_OBSERVED || state == TARGET_CONFIRMED || state == TARGET_DIFFERENT);
  }
  const char *name(uint32_t now, uint32_t maxAge) const
  {
    switch (state)
    {
      case TARGET_QUEUED: return "queued";
      case TARGET_WRITTEN: return "written";
      case TARGET_READ_FAILED: return "read_failed";
      case TARGET_WRITE_FAILED: return "write_failed";
      default: break;
    }
    if (!(flags & HAS_READ)) return "not_read";
    if (!fresh(now, maxAge)) return "stale";
    if (state == TARGET_CONFIRMED) return "confirmed";
    return state == TARGET_DIFFERENT ? "different" : "observed";
  }
};

#endif
