#ifndef ESPALTHERMA_DETECT_H
#define ESPALTHERMA_DETECT_H

// Heat pump model detection, from a survey (survey.h).
//  1. Known fingerprints (data/fingerprints.json): the identification key names the model.
//  2. The indoor unit AS number (registry 0x63), looked up in the table of known AS numbers
//     (data/as_numbers.json): its candidate definitions. One candidate: that is the model.
//  3. The candidates (or every model when the AS number is unknown) are scored against the survey:
//     - registries the model reads must answer; family registries (0xA0, 0xA1, 0x65) the model does not know should not,
//     - replies must be long enough for the model, ideally exactly as long as the model reads,
//     - values at the model's temperature/voltage/current/flow slots must be plausible.
// Models that decode the recommended values the same way are equivalent: the confidence is the margin
// between the best model and the best non-equivalent one.
//
// No Arduino dependency, for unit tests.

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "survey.h"
#include "catalog.h"

enum DetectConfidence
{
  DETECT_NONE = 0, // no answer from the heat pump
  DETECT_LOW,
  DETECT_MEDIUM,
  DETECT_HIGH,
};

#define DETECT_CANDIDATES 3

struct DetectResult
{
  int model; // best model, -1 when none
  DetectConfidence confidence;
  bool fingerprint;       // identified by a known fingerprint
  uint64_t asCandidates;  // models of the AS number, 0 when unknown
  int margin;             // score gap to the best non-equivalent model
  int candidates[DETECT_CANDIDATES];
  int scores[DETECT_CANDIDATES];
  int allScores[CATALOG_MODEL_COUNT]; // score of every model, -10000 when excluded
};

// Score weights
#define DETECT_REG_ANSWERED 2
// Definitions list registries and offsets a given unit may not have (the real EPGA16 has no 0xA0/0xA1, and a
// shorter 0x63 than its definition reads): a missing registry or a short reply is a hint, not a proof.
#define DETECT_REG_MISSING -6
#define DETECT_REG_UNEXPECTED -8
#define DETECT_LEN_EXACT 4
#define DETECT_LEN_SHORT -4
#define DETECT_TEMP_OK 0
#define DETECT_TEMP_BAD -4
#define DETECT_VOLT_OK 2
#define DETECT_VOLT_BAD -6
#define DETECT_OTHER_OK 0
#define DETECT_OTHER_BAD -2

#define DETECT_CAP_OK 3
#define DETECT_CAP_BAD -12

#define DETECT_MARGIN_HIGH 8
#define DETECT_MARGIN_MEDIUM 3

enum DetectQuantity
{
  QTY_NONE,
  QTY_TEMP,
  QTY_VOLTAGE,
  QTY_CURRENT,
  QTY_FLOW,
  QTY_FREQ,
  QTY_PRESSURE,
};

static bool detectContains(const char *label, const char *word)
{
  // Case insensitive strstr
  size_t n = strlen(word);
  for (const char *p = label; *p; p++)
  {
    size_t i = 0;
    while (i < n && p[i] && tolower((unsigned char)p[i]) == word[i])
      i++;
    if (i == n)
      return true;
  }
  return false;
}

static DetectQuantity detectQuantity(const CatalogEntry &e)
{
  const char *label = catalogLabel(e);
  if (detectContains(label, "voltage") && (e.conv == 101 || e.conv == 152 || e.conv == 105))
    return QTY_VOLTAGE;
  if (e.conv == 105 && detectContains(label, "l/min"))
    return QTY_FLOW;
  if (e.conv == 105 && detectContains(label, "temp"))
    return QTY_TEMP;
  if ((e.conv == 105 || e.conv == 161) && (detectContains(label, "(a)") || detectContains(label, "current")))
    return QTY_CURRENT;
  if (e.conv == 152 && detectContains(label, "(rps)"))
    return QTY_FREQ;
  if (e.conv == 105 && detectContains(label, "pressure") && !detectContains(label, "(t)"))
    return QTY_PRESSURE;
  return QTY_NONE;
}

// Decodes like converters.h for the conversions the plausibility checks use.
static bool detectDecode(const uint8_t *p, uint16_t conv, uint8_t size, double &v)
{
  unsigned int le = size == 2 ? (unsigned int)(p[1] << 8 | p[0]) : p[0];
  unsigned int be = size == 2 ? (unsigned int)(p[0] << 8 | p[1]) : p[0];
  int sle = size == 2 && (le & 0x8000) ? (int)le - 0x10000 : (int)le;
  switch (conv)
  {
  case 101:
    v = sle;
    return true;
  case 105:
    v = sle * 0.1;
    return true;
  case 151:
    v = le;
    return true;
  case 152:
    v = be;
    return true;
  case 161:
    v = be * 0.5;
    return true;
  default:
    return false;
  }
}

static int detectPlausibility(DetectQuantity q, double v)
{
  switch (q)
  {
  case QTY_TEMP:
    return v >= -45 && v <= 110 ? DETECT_TEMP_OK : DETECT_TEMP_BAD;
  case QTY_VOLTAGE:
    return v >= 90 && v <= 270 ? DETECT_VOLT_OK : DETECT_VOLT_BAD;
  case QTY_CURRENT:
    return v >= 0 && v <= 60 ? DETECT_OTHER_OK : DETECT_OTHER_BAD;
  case QTY_FLOW:
    return v >= 0 && v <= 150 ? DETECT_OTHER_OK : DETECT_OTHER_BAD;
  case QTY_FREQ:
    return v >= 0 && v <= 250 ? DETECT_OTHER_OK : DETECT_OTHER_BAD;
  case QTY_PRESSURE:
    return v >= -1 && v <= 60 ? DETECT_OTHER_OK : DETECT_OTHER_BAD;
  default:
    return 0;
  }
}

// Registries only some families have: answering one of them tells the model should know it.
static bool detectFamilyRegistry(uint8_t reg)
{
  return reg == 0xA0 || reg == 0xA1 || reg == 0x65;
}

int detectScore(const Survey &s, int model)
{
  const CatalogModel &m = CATALOG_MODELS[model];
  if (m.protocol != s.protocol)
    return -10000;

  if (s.protocol == 'S')
  {
    // PROTOCOL_S has 0x50, the ROTEX variant 0x56
    bool rotex = m.family == 'R';
    int score = 0;
    score += s.has(0x50) != rotex ? 10 : -10;
    score += s.has(0x56) == rotex ? 5 : 0;
    return score;
  }

  // Registries used by the model, and how far it reads into each
  uint8_t maxEnd[256];
  memset(maxEnd, 0, sizeof(maxEnd));
  bool used[256];
  memset(used, 0, sizeof(used));
  for (int i = 0; i < CATALOG_ENTRY_COUNT; i++)
  {
    const CatalogEntry &e = CATALOG_ENTRIES[i];
    if (!catalogInModel(e, model) || e.size == 0)
      continue;
    used[e.reg] = true;
    if (e.offset + e.size > maxEnd[e.reg])
      maxEnd[e.reg] = e.offset + e.size;
  }

  int score = 0;

  // Outdoor capacity (registry 0x00 offset 12, kW x10) against the range of the definition
  int capacity = s.byteAt(0x00, 12);
  if (capacity > 0 && m.capMax > 0)
    score += capacity >= (m.capMin - 1) * 10 && capacity <= (m.capMax + 1) * 10 ? DETECT_CAP_OK : DETECT_CAP_BAD;

  for (uint8_t i = 0; i < s.count; i++)
  {
    const SurveyReg &r = s.regs[i];
    if (used[r.id])
    {
      if (!r.answered)
      {
        score += DETECT_REG_MISSING;
        continue;
      }
      score += DETECT_REG_ANSWERED;
      if (r.len < maxEnd[r.id])
        score += DETECT_LEN_SHORT;
      else if (r.len == maxEnd[r.id])
        score += DETECT_LEN_EXACT;
    }
    else if (r.answered && detectFamilyRegistry(r.id))
    {
      score += DETECT_REG_UNEXPECTED;
    }
  }

  // Plausibility of the physical values the model would read
  for (int i = 0; i < CATALOG_ENTRY_COUNT; i++)
  {
    const CatalogEntry &e = CATALOG_ENTRIES[i];
    if (!catalogInModel(e, model) || e.size == 0)
      continue;
    const SurveyReg *r = s.find(e.reg);
    if (r == nullptr || !r->answered || e.offset + e.size > r->len)
      continue;
    DetectQuantity q = detectQuantity(e);
    double v;
    if (q != QTY_NONE && detectDecode(r->payload + e.offset, e.conv, e.size, v))
      score += detectPlausibility(q, v);
  }
  return score;
}

// Hash of the recommended values of a model: models with the same hash read the same values the same way.
static uint32_t detectDecodeHash(int model)
{
  uint32_t h = 2166136261u;
  for (int i = 0; i < CATALOG_ENTRY_COUNT; i++)
  {
    const CatalogEntry &e = CATALOG_ENTRIES[i];
    if (!catalogInModel(e, model) || !(e.flags & CATALOG_FLAG_RECOMMENDED))
      continue;
    uint32_t k = catalogKey(e);
    for (int b = 0; b < 4; b++)
    {
      h ^= (k >> (8 * b)) & 0xff;
      h *= 16777619u;
    }
  }
  return h;
}

static int detectPopcount(uint64_t v)
{
  int n = 0;
  for (; v; v &= v - 1)
    n++;
  return n;
}

// Candidate models of the indoor unit AS number, 0 when it is not known.
uint64_t detectAsCandidates(const Survey &s)
{
  AsNumber as;
  if (s.protocol != 'I' || !surveyAsNumber(s, as))
    return 0;
  uint64_t full = 0, first = 0, body = 0;
  for (int i = 0; i < CATALOG_AS_COUNT; i++)
  {
    const CatalogAsNumber &a = CATALOG_AS[i];
    if (a.body != as.body)
      continue;
    if (a.suffix1 == 0xFF)
      body = a.models; // spare part board, or a suffix newer than the table
    else if (a.suffix1 == as.suffix1 && a.suffix2 == 0xFF)
      first = a.models;
    else if (a.suffix1 == as.suffix1 && a.suffix2 == as.suffix2)
      full = a.models;
  }
  return full ? full : (first ? first : body);
}

void detectModel(const Survey &s, DetectResult &r)
{
  memset(&r, 0, sizeof(r));
  r.model = -1;
  for (int i = 0; i < DETECT_CANDIDATES; i++)
    r.candidates[i] = -1;
  if (s.protocol == 0)
    return;

  // 1. Known fingerprint
  char key[96];
  surveyKey(s, key, sizeof(key));
  for (int i = 0; i < CATALOG_FINGERPRINT_COUNT; i++)
  {
    const CatalogFingerprint &fp = CATALOG_FINGERPRINTS[i];
    if (strncmp(key, fp.key, strlen(fp.key)) == 0)
    {
      r.model = fp.model;
      r.fingerprint = true;
      r.confidence = DETECT_HIGH;
      r.candidates[0] = fp.model;
      r.scores[0] = detectScore(s, fp.model);
      return;
    }
  }

  // 2. AS number: the candidates. The survey only ranks them.
  r.asCandidates = detectAsCandidates(s);

  // 3. Scoring: keep the best candidates
  int *scores = r.allScores;
  for (int i = 0; i < CATALOG_MODEL_COUNT; i++)
  {
    bool candidate = r.asCandidates == 0 || ((r.asCandidates >> i) & 1);
    scores[i] = candidate ? detectScore(s, i) : -10000;
    int score = scores[i];
    if (score <= -10000)
      continue;
    for (int c = 0; c < DETECT_CANDIDATES; c++)
    {
      if (r.candidates[c] < 0 || score > r.scores[c])
      {
        for (int k = DETECT_CANDIDATES - 1; k > c; k--)
        {
          r.candidates[k] = r.candidates[k - 1];
          r.scores[k] = r.scores[k - 1];
        }
        r.candidates[c] = i;
        r.scores[c] = score;
        break;
      }
    }
  }
  if (r.candidates[0] < 0)
    return;
  r.model = r.candidates[0];

  // Margin to the best model that would read the values differently
  uint32_t bestHash = detectDecodeHash(r.model);
  bool found = false;
  int runnerUp = 0;
  for (int i = 0; i < CATALOG_MODEL_COUNT; i++)
  {
    if (i == r.model || scores[i] <= -10000 || detectDecodeHash(i) == bestHash)
      continue;
    if (!found || scores[i] > runnerUp)
    {
      runnerUp = scores[i];
      found = true;
    }
  }
  r.margin = found ? scores[r.model] - runnerUp : 100; // single AS candidate (or equivalents only): 100
  r.confidence = r.margin >= DETECT_MARGIN_HIGH ? DETECT_HIGH : (r.margin >= DETECT_MARGIN_MEDIUM ? DETECT_MEDIUM : DETECT_LOW);
  // On Protocol I the rules alone are never sure: on a real unit they preferred a wrong model by a wide margin.
  // A high confidence needs a known fingerprint or a single AS number candidate; otherwise the user confirms.
  // (Protocol S has two definitions only, told apart by which registries answer.)
  if (s.protocol == 'I' && r.confidence == DETECT_HIGH && detectPopcount(r.asCandidates) != 1)
    r.confidence = DETECT_MEDIUM;
}

static const char *detectMethod(const DetectResult &r)
{
  if (r.fingerprint)
    return "fingerprint";
  if (r.asCandidates)
    return detectPopcount(r.asCandidates) == 1 ? "as_number" : "as_number+rules";
  return "rules";
}

static bool detectModelHasKey(int model, uint32_t key)
{
  for (int i = 0; i < CATALOG_ENTRY_COUNT; i++)
  {
    const CatalogEntry &e = CATALOG_ENTRIES[i];
    if (catalogInModel(e, model) && catalogKey(e) == key)
      return true;
  }
  return false;
}

// Values safe to publish for the detected model, as catalog keys.
// Returns -1 when all the recommended values of the model can be used (high confidence).
// Otherwise returns the number of keys written: the recommended values of the best model that every model
// scoring within DETECT_MARGIN_HIGH of it reads the same way (same registry, offset, conversion and size).
int detectSafeKeys(const DetectResult &r, uint32_t *keys, size_t max)
{
  if (r.model < 0)
    return 0;
  if (r.confidence == DETECT_HIGH)
    return -1;
  int best = r.allScores[r.model];
  size_t n = 0;
  for (int i = 0; i < CATALOG_ENTRY_COUNT && n < max; i++)
  {
    const CatalogEntry &e = CATALOG_ENTRIES[i];
    if (!catalogInModel(e, r.model) || !(e.flags & CATALOG_FLAG_RECOMMENDED) || (e.flags & CATALOG_FLAG_ALWAYS))
      continue;
    uint32_t key = catalogKey(e);
    bool common = true;
    for (size_t k = 0; k < n && common; k++)
      common = keys[k] != key; // already listed
    for (int m = 0; m < CATALOG_MODEL_COUNT && common; m++)
    {
      if (m == r.model || r.allScores[m] <= best - DETECT_MARGIN_HIGH)
        continue;
      common = detectModelHasKey(m, key);
    }
    if (common)
      keys[n++] = key;
  }
  return (int)n;
}

const char *detectConfidenceName(DetectConfidence c)
{
  switch (c)
  {
  case DETECT_HIGH:
    return "high";
  case DETECT_MEDIUM:
    return "medium";
  case DETECT_LOW:
    return "low";
  default:
    return "none";
  }
}

// {"model":"...","family":"G","confidence":"high","method":"rules","margin":12,"candidates":[{"model":"...","score":40},...]}
size_t detectToJson(const DetectResult &r, char *out, size_t size)
{
  size_t pos = 0;
  out[0] = 0;
  if (r.model < 0)
  {
    surveyAppend(out, size, pos, "{\"model\":null,\"confidence\":\"none\"}");
    return pos;
  }
  const CatalogModel &m = CATALOG_MODELS[r.model];
  surveyAppend(out, size, pos, "{\"model\":\"%s\",\"family\":\"%c\",\"confidence\":\"%s\",\"method\":\"%s\",\"margin\":%d,\"candidates\":[",
               m.name, m.family, detectConfidenceName(r.confidence), detectMethod(r), r.margin);
  for (int c = 0; c < DETECT_CANDIDATES && r.candidates[c] >= 0; c++)
  {
    surveyAppend(out, size, pos, "%s{\"model\":\"%s\",\"score\":%d}", c ? "," : "", CATALOG_MODELS[r.candidates[c]].name, r.scores[c]);
  }
  surveyAppend(out, size, pos, "]}");
  return pos;
}

#endif
