#ifndef ESPALTHERMA_LAYOUTFIX_H
#define ESPALTHERMA_LAYOUTFIX_H

// Per-registry layout check: corrects a definition that does not match the unit.
// Some registries (0x21, 0x30, 0x00, 0x61...) have 2-byte values at different offsets depending on the
// definition. A wrong layout reads each value across two neighbours: absurd numbers that jump by 25.6 when a
// neighbouring byte changes by one. On a real EPGA16, 0x21 matches the Gen-1 layout, not its own definition.
// For those registries, every candidate layout is scored on the real replies:
//  - plausible: values in a physical range,
//  - steady: small changes between two polls,
//  - mirrored: values equal to a sensor of another registry (the same sensor is often served twice),
// and a registry switches to another layout only on clear evidence. Registries without evidence (all zero
// while the unit is idle) are never touched.
//
// No Arduino dependency, for unit tests.

#include <ctype.h>
#include <math.h>
#include <stdint.h>
#include <string.h>
#include "catalog.h"

#define LAYOUT_MAX_REGS 6
#define LAYOUT_MAX_CANDIDATES 6
#define LAYOUT_MAX_SLOTS 12
#define LAYOUT_MIN_POLLS 4
#define LAYOUT_CACHE_REGS 20
#define LAYOUT_PAYLOAD 60

struct LayoutSlot
{
  uint8_t offset;
  uint16_t conv;
};

struct LayoutCandidate
{
  int model;      // the model with this layout most similar to the active one: its entries replace the registry's
  int similarity; // entries it shares with the active model
  uint8_t slotCount;
  LayoutSlot slots[LAYOUT_MAX_SLOTS];
  float prev[LAYOUT_MAX_SLOTS];
  bool hasPrev[LAYOUT_MAX_SLOTS];
  uint16_t checks, plausible, steps, jumps, mirrors, nonZero;
};

struct LayoutCheck
{
  uint8_t reg;
  uint8_t count; // candidates; [0] is the layout of the active model
  uint8_t polls;
  LayoutCandidate c[LAYOUT_MAX_CANDIDATES];
};

struct LayoutChecker
{
  int model;
  uint8_t count;
  LayoutCheck regs[LAYOUT_MAX_REGS];
};

// Replies of one poll cycle, for the mirror test
struct LayoutCycle
{
  uint8_t count;
  uint8_t reg[LAYOUT_CACHE_REGS];
  uint8_t len[LAYOUT_CACHE_REGS];
  uint8_t data[LAYOUT_CACHE_REGS][LAYOUT_PAYLOAD];

  void clear() { count = 0; }
  void add(uint8_t r, const uint8_t *payload, uint8_t n)
  {
    if (count >= LAYOUT_CACHE_REGS)
      return;
    reg[count] = r;
    len[count] = n > LAYOUT_PAYLOAD ? LAYOUT_PAYLOAD : n;
    memcpy(data[count], payload, len[count]);
    count++;
  }
};

// Decodes a 2-byte value like converters.h; false for conversions the check does not use
static bool layoutDecode(const uint8_t *p, uint16_t conv, float &v)
{
  int le = p[0] | p[1] << 8, be = p[0] << 8 | p[1];
  int sle = le & 0x8000 ? le - 0x10000 : le, sbe = be & 0x8000 ? be - 0x10000 : be;
  switch (conv)
  {
  case 105: case 107: case 405: v = sle * 0.1f; return true;
  case 106: case 108: case 406: v = sbe * 0.1f; return true;
  case 117: v = sle * 0.01f; return true;
  case 118: v = sbe * 0.01f; return true;
  case 101: v = (float)sle; return true;
  case 102: v = (float)sbe; return true;
  case 151: v = (float)le; return true;
  case 152: v = (float)be; return true;
  default: return false;
  }
}

static bool layoutScaled(uint16_t conv) { return conv != 101 && conv != 102 && conv != 151 && conv != 152; }

static bool layoutPlausible(uint16_t conv, float v)
{
  if (layoutScaled(conv))
    return v >= -50 && v <= 150;
  if (conv == 101 || conv == 102)
    return v >= -1000 && v <= 1000;
  return v < 5000; // counters: pulses, frequencies
}

static float layoutJumpLimit(uint16_t conv) { return layoutScaled(conv) ? 5.0f : (conv == 151 || conv == 152 ? 1000.0f : 50.0f); }

// "Not in use" slots, case insensitive
static bool layoutUnused(const char *label)
{
  const char *w = "not in use";
  for (const char *p = label; *p; p++)
  {
    size_t i = 0;
    while (w[i] && p[i] && tolower((unsigned char)p[i]) == w[i])
      i++;
    if (!w[i])
      return true;
  }
  return false;
}

// 2-byte numeric slots of a model in a registry, by offset
static uint8_t layoutSlotsOf(int model, uint8_t reg, LayoutSlot *slots)
{
  uint8_t n = 0;
  for (int i = 0; i < CATALOG_ENTRY_COUNT && n < LAYOUT_MAX_SLOTS; i++)
  {
    const CatalogEntry &e = CATALOG_ENTRIES[i];
    float dummy;
    uint8_t probe[2] = {0, 0};
    if (!catalogInModel(e, model) || e.reg != reg || e.size != 2 || !layoutDecode(probe, e.conv, dummy) || layoutUnused(catalogLabel(e)))
      continue;
    bool dup = false;
    for (uint8_t k = 0; k < n && !dup; k++)
      dup = slots[k].offset == e.offset;
    if (dup)
      continue;
    uint8_t k = n++;
    while (k > 0 && slots[k - 1].offset > e.offset)
    {
      slots[k] = slots[k - 1];
      k--;
    }
    slots[k] = {e.offset, e.conv};
  }
  return n;
}

static bool layoutSame(const LayoutSlot *a, uint8_t na, const LayoutSlot *b, uint8_t nb)
{
  if (na != nb)
    return false;
  for (uint8_t i = 0; i < na; i++)
    if (a[i].offset != b[i].offset || layoutScaled(a[i].conv) != layoutScaled(b[i].conv))
      return false;
  return true;
}

// A 2-byte value of one layout starts one byte off a 2-byte value of the other
static bool layoutConflict(const LayoutSlot *a, uint8_t na, const LayoutSlot *b, uint8_t nb)
{
  for (uint8_t i = 0; i < na; i++)
    for (uint8_t j = 0; j < nb; j++)
      if (a[i].offset + 1 == b[j].offset || b[j].offset + 1 == a[i].offset)
        return true;
  return false;
}

// Values two models define the same way (slot and label): the most similar model with a layout gives the labels
// of the corrected registry. Compared by content: the catalog only merges the entries of a family.
static int layoutSimilarity(int a, int b)
{
  int n = 0;
  for (int i = 0; i < CATALOG_ENTRY_COUNT; i++)
  {
    const CatalogEntry &ea = CATALOG_ENTRIES[i];
    if (!catalogInModel(ea, a))
      continue;
    uint32_t key = catalogKey(ea);
    for (int j = 0; j < CATALOG_ENTRY_COUNT; j++)
    {
      const CatalogEntry &eb = CATALOG_ENTRIES[j];
      if (eb.label == ea.label && catalogInModel(eb, b) && catalogKey(eb) == key)
      {
        n++;
        break;
      }
    }
  }
  return n;
}

// Sets up the check for the active model: the registries where another model puts 2-byte values at other offsets.
void layoutCheckInit(LayoutChecker &lc, int model)
{
  memset(&lc, 0, sizeof(lc));
  lc.model = model;
  if (model < 0 || CATALOG_MODELS[model].protocol != 'I')
    return;
  bool seen[256];
  memset(seen, 0, sizeof(seen));
  for (int i = 0; i < CATALOG_ENTRY_COUNT && lc.count < LAYOUT_MAX_REGS; i++)
  {
    const CatalogEntry &e = CATALOG_ENTRIES[i];
    if (!catalogInModel(e, model) || seen[e.reg])
      continue;
    seen[e.reg] = true;
    LayoutCheck &rc = lc.regs[lc.count];
    memset(&rc, 0, sizeof(rc));
    rc.reg = e.reg;
    LayoutCandidate &active = rc.c[rc.count++];
    active.model = model;
    active.slotCount = layoutSlotsOf(model, e.reg, active.slots);
    if (active.slotCount == 0)
      continue;
    for (int m = 0; m < CATALOG_MODEL_COUNT && rc.count < LAYOUT_MAX_CANDIDATES; m++)
    {
      if (m == model || CATALOG_MODELS[m].protocol != 'I')
        continue;
      LayoutSlot slots[LAYOUT_MAX_SLOTS];
      uint8_t n = layoutSlotsOf(m, e.reg, slots);
      if (n == 0 || !layoutConflict(active.slots, active.slotCount, slots, n))
        continue;
      int similarity = layoutSimilarity(m, model);
      int known = -1;
      for (uint8_t k = 0; k < rc.count && known < 0; k++)
        if (layoutSame(rc.c[k].slots, rc.c[k].slotCount, slots, n))
          known = k;
      if (known >= 0)
      {
        if (known > 0 && similarity > rc.c[known].similarity)
        {
          rc.c[known].model = m;
          rc.c[known].similarity = similarity;
        }
        continue;
      }
      LayoutCandidate &c = rc.c[rc.count++];
      c.model = m;
      c.similarity = similarity;
      c.slotCount = n;
      memcpy(c.slots, slots, sizeof(slots));
    }
    if (rc.count > 1)
      lc.count++; // only registries with conflicting layouts are checked
  }
}

static bool layoutMirrored(const LayoutCycle &cycle, uint8_t reg, float v)
{
  if (fabsf(v) < 5 || fabsf(v) > 100)
    return false;
  for (uint8_t r = 0; r < cycle.count; r++)
  {
    if (cycle.reg[r] == reg)
      continue;
    for (uint8_t o = 0; o + 1 < cycle.len[r]; o++)
    {
      float w;
      layoutDecode(cycle.data[r] + o, 105, w);
      if (fabsf(w - v) < 0.05f)
        return true;
    }
  }
  return false;
}

// Scores every candidate on the replies of one poll cycle
void layoutCheckCycle(LayoutChecker &lc, const LayoutCycle &cycle)
{
  for (uint8_t i = 0; i < lc.count; i++)
  {
    LayoutCheck &rc = lc.regs[i];
    int idx = -1;
    for (uint8_t r = 0; r < cycle.count && idx < 0; r++)
      if (cycle.reg[r] == rc.reg)
        idx = r;
    if (idx < 0)
      continue;
    rc.polls++;
    for (uint8_t k = 0; k < rc.count; k++)
    {
      LayoutCandidate &c = rc.c[k];
      for (uint8_t s = 0; s < c.slotCount; s++)
      {
        const LayoutSlot &slot = c.slots[s];
        float v;
        if (slot.offset + 2 > cycle.len[idx] || !layoutDecode(cycle.data[idx] + slot.offset, slot.conv, v))
          continue;
        c.checks++;
        c.plausible += layoutPlausible(slot.conv, v);
        c.nonZero += v != 0;
        if (layoutScaled(slot.conv) && layoutMirrored(cycle, rc.reg, v))
          c.mirrors++;
        if (c.hasPrev[s])
        {
          c.steps++;
          c.jumps += fabsf(v - c.prev[s]) > layoutJumpLimit(slot.conv);
        }
        c.prev[s] = v;
        c.hasPrev[s] = true;
      }
    }
  }
}

// The candidate a registry should switch to, -1 to keep the active layout.
int layoutCheckDecide(const LayoutCheck &rc)
{
  if (rc.polls < LAYOUT_MIN_POLLS || rc.count < 2)
    return -1;
  const LayoutCandidate &a = rc.c[0];
  if (a.checks == 0 || a.plausible * 10 >= a.checks * 9)
    return -1; // the active layout reads plausible values: keep it
  int best = -1;
  for (uint8_t k = 1; k < rc.count; k++)
  {
    const LayoutCandidate &c = rc.c[k];
    bool clean = c.checks > 0 && c.plausible == c.checks && c.jumps == 0 && c.nonZero >= 2 * rc.polls;
    bool supported = a.jumps >= 2 || c.mirrors * 2 >= rc.polls;
    if (!clean || !supported)
      continue;
    if (best < 0 || c.mirrors > rc.c[best].mirrors || (c.mirrors == rc.c[best].mirrors && c.slotCount > rc.c[best].slotCount))
      best = k;
  }
  return best;
}

#endif
