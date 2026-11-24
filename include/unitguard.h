#ifndef ESPALTHERMA_UNITGUARD_H
#define ESPALTHERMA_UNITGUARD_H

#include <stdint.h>
#include <string.h>

// While the unit restarts (its user interface does it after a change of field settings, and so does a power cut),
// the board goes on answering every registry with a valid frame, and what it answers is zeros, whole or in part, for
// about two minutes (#714). No registry tells by itself: 0x20 keeps a last byte, 0x10, 0x21 and 0x30 read all zero
// long after the first replies, and the indoor side's (0x60 to 0x62) lose a few bytes for one poll.
// Registry 0x00, the outdoor unit's identification, does tell: all zero at the first bad reply, filled again from
// its end, and complete only when every other registry is right again (three restarts read every 5 s on an
// EHBX16CB3V). So it is read before and after the others at each poll, and the poll is kept back, neither converted
// nor published, while one of the two reads lacks a byte the registry held on the running unit.
// A unit can answer these zeros for good (an outdoor unit without power): after UNIT_GUARD_MAX_MS the replies are
// taken as they come again, and what the registry holds then is what it is compared with from there on.

#define UNIT_GUARD_PAYLOAD 32
#define UNIT_GUARD_MAX_MS 300000UL
#define UNIT_GUARD_UNANSWERED 3 // polls without an answer from a registry that never answered: no longer asked

enum UnitGuardVerdict
{
  UNIT_GUARD_PASS, // the poll's replies are readings
  UNIT_GUARD_HOLD  // the unit restarts: the poll is dropped, the readings stay what they were
};

struct UnitGuard
{
  uint8_t known[UNIT_GUARD_PAYLOAD]; // registry 0x00 of the running unit
  uint8_t knownLen = 0;              // 0: not read with content yet, nothing to compare with
  bool holding = false;
  unsigned long since = 0; // start of the hold
  bool everAnswered = false;
  uint8_t unanswered = 0;
  // What the last poll changed, for the event history
  bool began = false, ended = false, gaveUp = false;

  // The values to read changed (another model, a registry read with another layout): the registry is asked again
  void retry() { unanswered = 0; }

  // Whether the registry is worth a query: a unit that never answers it costs a timeout at every poll
  bool wanted() const { return everAnswered || unanswered < UNIT_GUARD_UNANSWERED; }

  void answered(bool ok)
  {
    if (ok)
    {
      everAnswered = true;
      unanswered = 0;
    }
    else if (unanswered < 255)
      unanswered++;
  }

  // A byte the registry held on the running unit reads zero (or is missing)
  bool lacks(const uint8_t *payload, uint8_t len) const
  {
    for (uint8_t i = 0; i < knownLen; i++)
      if (known[i] != 0 && (i >= len || payload[i] == 0))
        return true;
    return false;
  }

  void learn(const uint8_t *payload, uint8_t len)
  {
    if (len > UNIT_GUARD_PAYLOAD)
      len = UNIT_GUARD_PAYLOAD;
    bool content = false;
    for (uint8_t i = 0; i < len; i++)
      content = content || payload[i] != 0;
    knownLen = content ? len : 0;
    memcpy(known, payload, knownLen);
  }

  // The two reads of registry 0x00 around a poll (nullptr: no answer) and the time: what to do with the poll
  UnitGuardVerdict poll(const uint8_t *before, uint8_t beforeLen, const uint8_t *after, uint8_t afterLen, unsigned long now)
  {
    began = ended = gaveUp = false;
    const uint8_t *last = after ? after : before;
    const uint8_t lastLen = after ? afterLen : beforeLen;
    // Without an answer nothing is known: what was decided at the last poll stands
    const bool incomplete = last == nullptr ? holding : (before && lacks(before, beforeLen)) || (after && lacks(after, afterLen));
    if (!incomplete)
    {
      ended = holding;
      holding = false;
      if (last)
        learn(last, lastLen);
      return UNIT_GUARD_PASS;
    }
    if (!holding)
    {
      holding = true;
      since = now;
      began = true;
    }
    if (now - since < UNIT_GUARD_MAX_MS)
      return UNIT_GUARD_HOLD;
    holding = false;
    gaveUp = true;
    if (last)
      learn(last, lastLen);
    else
      knownLen = 0;
    return UNIT_GUARD_PASS;
  }
};

#endif
