#ifndef ESPALTHERMA_SURVEY_H
#define ESPALTHERMA_SURVEY_H

// Heat pump survey, the first step of the auto-detection.
// Finds the protocol the heat pump speaks, reads once every registry used by the definition files,
// and extracts the identification fields (fingerprint) that every Altherma indoor unit reports.
// The JSON report is published on espaltherma/detect, and is the payload of the opt-in telemetry.
//
// No Arduino dependency: the registry query is injected, so recorded dumps can be replayed in unit tests.

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#define SURVEY_MAX_REGS 24
#define SURVEY_MAX_PAYLOAD 60
#define SURVEY_BUFFER_SIZE 64
#define SURVEY_JSON_VERSION 1

struct SurveyReg
{
  uint8_t id;
  bool answered;
  uint8_t len; // payload length, the label offsets index this payload
  uint8_t payload[SURVEY_MAX_PAYLOAD];
};

struct Survey
{
  char protocol; // 'I', 'S', or 0 when the heat pump did not answer
  uint8_t count;
  SurveyReg regs[SURVEY_MAX_REGS];

  const SurveyReg *find(uint8_t id) const
  {
    for (uint8_t i = 0; i < count; i++)
    {
      if (regs[i].id == id)
        return &regs[i];
    }
    return nullptr;
  }

  bool has(uint8_t id) const
  {
    const SurveyReg *r = find(id);
    return r != nullptr && r->answered;
  }

  // Payload byte at the given label offset, -1 when not available
  int byteAt(uint8_t id, uint8_t offset) const
  {
    const SurveyReg *r = find(id);
    if (r == nullptr || !r->answered || offset >= r->len)
      return -1;
    return r->payload[offset];
  }
};

// Sends a registry query and fills buffer (SURVEY_BUFFER_SIZE bytes) with the full reply frame.
// Returns false when the heat pump gave no valid answer.
typedef bool (*SurveyQueryFn)(uint8_t regID, unsigned char *buffer, char protocol);

// Registries of the Protocol I definition files. 0x61..0x6F are walked after 0x60 until the first
// missing one, like D-Checker does.
static const uint8_t SURVEY_REGS_I[] = {0x00, 0x10, 0x11, 0x20, 0x21, 0x30, 0xA0, 0xA1};
static const uint8_t SURVEY_REGS_S[] = {0x50, 0x54, 0x55, 0x56};

static uint8_t surveyReplyLenS(uint8_t regID)
{
  switch (regID)
  {
  case 0x50:
    return 6;
  case 0x56:
    return 4;
  default:
    return 18;
  }
}

// Reads one registry into the survey. Two attempts, so that one lost frame does not look like a missing registry.
static bool surveyRead(Survey &s, SurveyQueryFn query, uint8_t regID, char protocol)
{
  if (s.count >= SURVEY_MAX_REGS)
    return false;
  SurveyReg &r = s.regs[s.count++];
  memset(&r, 0, sizeof(r));
  r.id = regID;

  unsigned char buf[SURVEY_BUFFER_SIZE];
  for (int attempt = 0; attempt < 2 && !r.answered; attempt++)
  {
    memset(buf, 0, sizeof(buf));
    if (!query(regID, buf, protocol))
      continue;
    if (protocol == 'I')
    {
      // 0x40 regID L payload[L-2] crc
      if (buf[0] != 0x40 || buf[1] != regID || buf[2] < 2)
        continue;
      r.len = buf[2] - 2;
      if (r.len > SURVEY_MAX_PAYLOAD)
        r.len = SURVEY_MAX_PAYLOAD;
      memcpy(r.payload, buf + 3, r.len);
    }
    else
    {
      // regID payload crc, the length depends on the registry
      if (buf[0] != regID)
        continue;
      r.len = surveyReplyLenS(regID) - 2;
      memcpy(r.payload, buf + 1, r.len);
    }
    r.answered = true;
  }
  return r.answered;
}

void surveyRun(Survey &s, SurveyQueryFn query)
{
  memset(&s, 0, sizeof(s));

  // Protocol I: 0x60 is D-Checker's probe, all Altherma indoor units answer it.
  if (surveyRead(s, query, 0x60, 'I'))
  {
    s.protocol = 'I';
    for (uint8_t reg : SURVEY_REGS_I)
    {
      surveyRead(s, query, reg, 'I');
    }
    for (uint8_t reg = 0x61; reg <= 0x6F; reg++)
    {
      if (!surveyRead(s, query, reg, 'I'))
        break;
    }
    return;
  }

  // Protocol S: 0x53 is D-Checker's probe.
  s.count = 0;
  if (surveyRead(s, query, 0x53, 'S'))
  {
    s.protocol = 'S';
    for (uint8_t reg : SURVEY_REGS_S)
    {
      surveyRead(s, query, reg, 'S');
    }
    return;
  }

  s.count = 0; // No answer at all
}

// printf-style append that never overflows out
static void surveyAppend(char *out, size_t size, size_t &pos, const char *fmt, ...)
{
  if (pos >= size)
    return;
  va_list args;
  va_start(args, fmt);
  int n = vsnprintf(out + pos, size - pos, fmt, args);
  va_end(args);
  if (n > 0)
    pos += (size_t)n < size - pos ? (size_t)n : size - pos - 1;
}

// Appends the bytes [offset, offset+count) of a registry as hex, or "--" per missing byte.
static void surveyAppendHex(const Survey &s, uint8_t reg, uint8_t offset, uint8_t count, char *out, size_t size, size_t &pos)
{
  for (uint8_t i = 0; i < count; i++)
  {
    int b = s.byteAt(reg, offset + i);
    if (b < 0)
      surveyAppend(out, size, pos, "--");
    else
      surveyAppend(out, size, pos, "%02X", b);
  }
}

// Two decimal digits of a byte, one per nibble; -1 when a nibble is not a digit
static int surveyBcd(int b)
{
  if (b < 0 || (b >> 4) > 9 || (b & 0xF) > 9)
    return -1;
  return (b >> 4) * 10 + (b & 0xF);
}

struct AsNumber
{
  uint32_t body; // 7 digits, eg. 1708171 for AS1708171-30 F
  int suffix1;   // first suffix digit
  int suffix2;   // second suffix digit, -1 when it cannot be told
  char letter;   // revision letter, ' ' when none
};

// Indoor unit AS number, from registry 0x63 which holds its printed digits in order, one per nibble.
// AS1708171-30 F reads 01 70 81 71 03 06 at offsets 2-7 (a real unit).
// Offset 7 low nibble is the revision letter code (6 = F). The second suffix digit is the high nibble of offset 6
// or of offset 7 (not told apart yet: that unit has suffix 30); the other one carries the high bit of the letter
// code, 0 for the letters A-Q, which are the only ones the known boards use. So the non-zero nibble is the digit.
bool surveyAsNumber(const Survey &s, AsNumber &as)
{
  int d1 = s.byteAt(0x63, 2);
  int d23 = surveyBcd(s.byteAt(0x63, 3));
  int d45 = surveyBcd(s.byteAt(0x63, 4));
  int d67 = surveyBcd(s.byteAt(0x63, 5));
  int o6 = s.byteAt(0x63, 6);
  int o7 = s.byteAt(0x63, 7);
  if (d1 < 1 || d1 > 9 || d23 < 0 || d45 < 0 || d67 < 0 || o6 < 0 || (o6 & 0xF) > 9)
    return false;
  as.body = (uint32_t)d1 * 1000000 + d23 * 10000 + d45 * 100 + d67;
  as.suffix1 = o6 & 0xF;
  as.suffix2 = -1;
  as.letter = ' ';
  if (o7 >= 0)
  {
    int hi6 = o6 >> 4, hi7 = o7 >> 4;
    if (hi6 == 0 || hi7 == 0)
    {
      int digit = hi6 | hi7;
      as.suffix2 = digit <= 9 ? digit : -1;
      int code = o7 & 0xF; // Daikin revision letters, without I and O
      if (code >= 1)
        as.letter = "ABCDEFGHJKLMNPQ"[code - 1];
    }
  }
  return true;
}

// Outdoor unit P-number from registry 0x11, same order: 1P705348-1x reads 01 70 53 48 01 .. at offsets 0-5.
// Report only: it is not in the AS number table (outdoor board part number).
static bool surveyPNumber(const Survey &s, char *out, size_t size)
{
  int p1 = s.byteAt(0x11, 0);
  int a = surveyBcd(s.byteAt(0x11, 1));
  int b = surveyBcd(s.byteAt(0x11, 2));
  int c = surveyBcd(s.byteAt(0x11, 3));
  int q = s.byteAt(0x11, 4);
  if (p1 < 0 || p1 > 9 || a < 0 || b < 0 || c < 0 || q < 0 || (q & 0xF) > 9)
    return false;
  snprintf(out, size, "%dP%02d%02d%02d-%d", p1, a, b, c, q & 0xF);
  return true;
}

static void surveyAppendByteField(const Survey &s, const char *name, uint8_t reg, uint8_t offset, char *out, size_t size, size_t &pos)
{
  int b = s.byteAt(reg, offset);
  if (b < 0)
    surveyAppend(out, size, pos, "\"%s\":null,", name);
  else
    surveyAppend(out, size, pos, "\"%s\":%d,", name, b);
}

static void surveyAppendHexField(const Survey &s, const char *name, uint8_t reg, uint8_t offset, uint8_t count, char *out, size_t size, size_t &pos)
{
  if (!s.has(reg))
  {
    surveyAppend(out, size, pos, "\"%s\":null,", name);
    return;
  }
  surveyAppend(out, size, pos, "\"%s\":\"", name);
  surveyAppendHex(s, reg, offset, count, out, size, pos);
  surveyAppend(out, size, pos, "\",");
}

// Canonical identification key, eg. "I|63:0A1B2C3D4E05|60:9182|CAP:5A|11:010203040506|00:0102".
// Registry:hex of the identification bytes.
void surveyKey(const Survey &s, char *out, size_t size)
{
  size_t pos = 0;
  out[0] = 0;
  if (s.protocol == 'I')
  {
    surveyAppend(out, size, pos, "I|63:");
    surveyAppendHex(s, 0x63, 2, 6, out, size, pos);
    surveyAppend(out, size, pos, "|60:");
    surveyAppendHex(s, 0x60, 14, 2, out, size, pos);
    surveyAppend(out, size, pos, "|CAP:");
    surveyAppendHex(s, 0x60, 6, 1, out, size, pos);
    surveyAppend(out, size, pos, "|11:");
    surveyAppendHex(s, 0x11, 0, 6, out, size, pos);
    surveyAppend(out, size, pos, "|00:");
    surveyAppendHex(s, 0x00, 10, 2, out, size, pos);
  }
  else if (s.protocol == 'S')
  {
    surveyAppend(out, size, pos, "S|50:%d|56:%d", s.has(0x50) ? 1 : 0, s.has(0x56) ? 1 : 0);
  }
}

// Writes the survey report. detectJson (may be null) is inserted as the "detect" member.
// Returns the length written.
size_t surveyToJson(const Survey &s, char *out, size_t size, const char *fw, const char *board, const char *detectJson)
{
  size_t pos = 0;
  out[0] = 0;
  char protocol[2] = {s.protocol, 0};
  surveyAppend(out, size, pos, "{\"v\":%d,\"fw\":\"%s\",\"board\":\"%s\",\"protocol\":", SURVEY_JSON_VERSION, fw, board);
  if (s.protocol)
    surveyAppend(out, size, pos, "\"%s\",", protocol);
  else
    surveyAppend(out, size, pos, "null,");

  char key[96];
  surveyKey(s, key, sizeof(key));
  surveyAppend(out, size, pos, "\"key\":\"%s\",", key);

  if (s.protocol == 'I')
  {
    surveyAppend(out, size, pos, "\"id\":{");
    surveyAppendHexField(s, "iu_eeprom", 0x63, 2, 6, out, size, pos);
    AsNumber as;
    if (surveyAsNumber(s, as))
    {
      surveyAppend(out, size, pos, "\"iu_as\":\"AS%07lu-%d", (unsigned long)as.body, as.suffix1);
      if (as.suffix2 >= 0)
        surveyAppend(out, size, pos, "%d", as.suffix2);
      if (as.letter != ' ')
        surveyAppend(out, size, pos, " %c", as.letter);
      surveyAppend(out, size, pos, "\",");
    }
    surveyAppendHexField(s, "iu_sw", 0x60, 14, 2, out, size, pos);
    surveyAppendByteField(s, "iu_cap", 0x60, 6, out, size, pos);
    surveyAppendByteField(s, "iu_opt", 0x60, 13, out, size, pos);
    surveyAppendByteField(s, "iu_eeprom_ver", 0x60, 16, out, size, pos);
    surveyAppendHexField(s, "iu_code", 0x60, 4, 2, out, size, pos);
    surveyAppendHexField(s, "ou_eeprom", 0x11, 0, 6, out, size, pos);
    char pn[16];
    if (surveyPNumber(s, pn, sizeof(pn)))
      surveyAppend(out, size, pos, "\"ou_pn\":\"%s\",", pn);
    surveyAppendHexField(s, "ou_mpu", 0x00, 10, 2, out, size, pos);
    surveyAppendByteField(s, "ou_cap", 0x00, 12, out, size, pos);
    if (out[pos - 1] == ',')
      pos--;
    surveyAppend(out, size, pos, "},");
  }

  surveyAppend(out, size, pos, "\"regs\":{");
  for (uint8_t i = 0; i < s.count; i++)
  {
    const SurveyReg &r = s.regs[i];
    surveyAppend(out, size, pos, "%s\"%02X\":", i ? "," : "", r.id);
    if (!r.answered)
    {
      surveyAppend(out, size, pos, "null");
      continue;
    }
    surveyAppend(out, size, pos, "\"");
    for (uint8_t j = 0; j < r.len; j++)
    {
      surveyAppend(out, size, pos, "%02X", r.payload[j]);
    }
    surveyAppend(out, size, pos, "\"");
  }
  surveyAppend(out, size, pos, "}");

  if (detectJson != nullptr)
  {
    surveyAppend(out, size, pos, ",\"detect\":%s", detectJson);
  }
  surveyAppend(out, size, pos, "}");
  return pos;
}

#endif
