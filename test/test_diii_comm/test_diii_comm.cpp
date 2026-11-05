#include <stdlib.h>
#include <string.h>
#include <unity.h>

#include "diii_comm.h"

// Fake outdoor controller: a RAM image whose DIII slots fill on a timeline like the real unit's.
//  - hydro E0 page 0x80 in the rx slot (0x0C85) at second 22 of every minute, for 800 ms
//  - the outdoor's own E0 page 2 in the tx slot (0x0C25) at second 55, for 2 s
//  - 63 in the rx slot every 300 s (first at 100 s), for 10 s
// Each 'R' read costs 30 ms of the fake clock.
static unsigned long fakeNow = 0;
static unsigned long fakeClock() { return fakeNow; }

struct FakeOutdoor
{
  bool answer = true;
  const char *swId = "DP0009_003";
  float rated = 27.0f;       // 0x2B79
  uint8_t id63 = 0x01;       // 63 d[1]: another DIII system id
  uint16_t room63 = 0x1580;  // 63 BRC1 room, 8.8 (0: no thermistor)
  bool badWater = false;     // 63 with LWT 0: implausible
  uint8_t tank = 48;         // E0[21] DHW tank (0xAF = -47: no sensor)
  uint8_t bits0C = 0x04;     // E0[0C] bit 2: 3-way valve on hot water
  int reads = 0, otherCommands = 0, unsafeReads = 0;
  int windowReads = 0; // reads in the 60 s after both E0 phases are locked

  void reset() { *this = FakeOutdoor(); }

  void fill(uint16_t addr, uint8_t *out, unsigned long t)
  {
    static uint8_t ram[0x8000];
    memset(ram, 0, sizeof(ram));
    memcpy(ram + 0x0682, swId, strlen(swId));
    float cur = 4.4f;
    memcpy(ram + 0x2B73, &cur, 4);
    memcpy(ram + 0x2B79, &rated, 4);
    ram[0x2B7D] = 41; // 4.4 / 27 x 256
    unsigned long sec = t % 60000;
    if (sec >= 22000 && sec < 22800)
    {
      uint8_t *e = ram + 0x0C8B; // E0[0] = slot 0x0C85 + 6
      ram[0x0C8A] = 0xE0;
      e[4] = 0xFF;  // d[5]
      e[5] = 0x80;  // page
      e[7] = 48;    // EVL / 10
      e[8] = 0;     // EVH / 10
      e[9] = 0x0C;  // pump hi
      e[10] = 0x0C; // pump lo
      e[12] = bits0C;
      e[14] = 33;   // LWT
      e[15] = 28;   // RWT
      e[16] = 0x80 | 2; // room -2 (sign-magnitude)
      e[18] = 30;   // discharge/2
      e[19] = 40;   // fin
      e[0x21] = tank;
    }
    else if ((t >= 100000) && ((t - 100000) % 300000) < 10000)
    {
      ram[0x0C8A] = 0x63;
      ram[0x0C8B] = id63;
      ram[0x0C8F] = 0xFF; // d[5]
      ram[0x0C90] = 0x1C; // RWT 28.5
      ram[0x0C91] = 0x80;
      ram[0x0C92] = 0x1E; // liquid 30.25
      ram[0x0C93] = 0x40;
      ram[0x0C94] = badWater ? 0x00 : 0x21; // LWT 33.0
      ram[0x0C95] = 0x00;
      ram[0x0CA3] = room63 >> 8; // BRC1 21.5
      ram[0x0CA4] = room63 & 0xFF;
    }
    if (sec >= 55000 && sec < 57000)
    {
      ram[0x0C2A] = 0xE0;
      ram[0x0C2F] = 0x00;
      ram[0x0C30] = 0x02;
      ram[0x0C3A + 0x0D] = 32; // rps
      ram[0x0C3A + 0x0E] = 45; // fin
      ram[0x0C3A + 0x0F] = 5;  // A
    }
    memcpy(out, ram + addr, 16);
  }
};

static FakeOutdoor unit;

static size_t fakeExchange(const uint8_t *frame, size_t len, uint8_t *reply, size_t maxLen, unsigned long)
{
  fakeNow += 30;
  if (len != 5 || frame[0] != 0x04 || frame[1] != 0x52)
  {
    unit.otherCommands++;
    return 0;
  }
  uint16_t a = frame[2] | frame[3] << 8;
  unit.reads++;
  if (!DiiiComm::safeAddress(a))
    unit.unsafeReads++;
  if (!unit.answer || maxLen < 18)
    return 0;
  reply[0] = 0x52;
  unit.fill(a, reply + 1, fakeNow);
  uint8_t sum = 0;
  for (int i = 0; i < 17; i++)
    sum += reply[i];
  reply[17] = (uint8_t)~sum;
  return 18;
}

static DiiiComm reader;

static void runFor(unsigned long ms)
{
  unsigned long end = fakeNow + ms;
  while (fakeNow < end)
  {
    reader.tick();
    fakeNow += reader.idleMs();
  }
}

void setUp()
{
  unit.reset();
  fakeNow = 0;
  reader = DiiiComm();
  reader.exchange = fakeExchange;
  reader.clock = fakeClock;
}

void tearDown() {}

void test_silent_before_start_delay()
{
  runFor(89000);
  TEST_ASSERT_EQUAL(0, unit.reads);
}

void test_other_software_reads_nothing_more()
{
  unit.swId = "XX1234_001";
  runFor(20 * 60000UL);
  TEST_ASSERT_EQUAL(DIII_UNSUPPORTED, reader.status);
  TEST_ASSERT_TRUE(unit.reads <= 2 * 3); // identification only, every 10 min
}

void test_catches_all_frames_read_only()
{
  runFor(15 * 60000UL);
  TEST_ASSERT_EQUAL(DIII_READY, reader.status);
  TEST_ASSERT_EQUAL_STRING("DP0009_003", reader.softwareId);
  TEST_ASSERT_TRUE(reader.hydro.locked);
  TEST_ASSERT_TRUE(reader.outdoor.locked);
  TEST_ASSERT_EQUAL(0, unit.otherCommands);
  TEST_ASSERT_EQUAL(0, unit.unsafeReads);
  TEST_ASSERT_EQUAL(33, reader.v.lwt);
  TEST_ASSERT_EQUAL(28, reader.v.rwt);
  TEST_ASSERT_EQUAL(-2, reader.v.room);
  TEST_ASSERT_EQUAL(480, reader.v.evl);
  TEST_ASSERT_EQUAL(0x0C0C, reader.v.pump);
  TEST_ASSERT_EQUAL(32, reader.v.invRps);
  TEST_ASSERT_EQUAL(45, reader.v.invFin);
  TEST_ASSERT_EQUAL(5, reader.v.invAmp);
  TEST_ASSERT_TRUE(reader.v.f63At > 0);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 33.0f, reader.v.lwt63);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 28.5f, reader.v.rwt63);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 30.25f, reader.v.liq63);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 21.5f, reader.v.room63);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 4.4f, reader.v.current);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 27.0f, reader.v.rated);
  TEST_ASSERT_EQUAL(41, reader.v.load);
  TEST_ASSERT_TRUE(reader.r63Period > 290000 && reader.r63Period < 310000);
}

// A blocking registry poll every 30 s (2 s long) while learning, with the caller's +7 s shift while learning():
// the hydro phase must still lock (a plain 30 s poll can hide the < 1 s hydro frame every minute).
void test_learns_despite_a_30s_poll()
{
  // like hpTask: the period runs from the start of a poll; worst case, every poll starts at second 21.5 or
  // 51.5 and blocks the link for 2 s, over the whole hydro frame (second 22.0..22.8)
  unsigned long pollStart = 21500;
  unsigned long end = 15 * 60000UL;
  while (fakeNow < end)
  {
    if (fakeNow >= pollStart)
    {
      unsigned long start = fakeNow;
      reader.hold(1500, [](unsigned long ms) { fakeNow += ms; });
      fakeNow += 2000; // the poll itself (no reads by the reader)
      pollStart = start + 30000 + (reader.learning() ? 7000 : 0);
    }
    reader.tick();
    fakeNow += reader.idleMs();
  }
  TEST_ASSERT_TRUE(reader.hydro.locked);
  TEST_ASSERT_TRUE(reader.outdoor.locked);
  TEST_ASSERT_FALSE(reader.learning());
}

void test_locked_traffic_is_low()
{
  runFor(15 * 60000UL);
  TEST_ASSERT_TRUE(reader.hydro.locked && reader.outdoor.locked && reader.r63Period);
  int before = unit.reads;
  runFor(5 * 60000UL);
  int perMinute = (unit.reads - before) / 5;
  TEST_ASSERT_TRUE(perMinute <= 60);
  TEST_ASSERT_TRUE(perMinute >= 4);
}

void test_json_keys()
{
  runFor(15 * 60000UL);
  char buf[1024] = "";
  reader.appendJson(buf, sizeof(buf));
  TEST_ASSERT_NOT_NULL(strstr(buf, "\"E0: Leaving water (C)\":33,"));
  TEST_ASSERT_NOT_NULL(strstr(buf, "\"E0: Remote room temp (C)\":-2,"));
  TEST_ASSERT_NOT_NULL(strstr(buf, "\"E0: INV current (A)\":5,"));
  TEST_ASSERT_NOT_NULL(strstr(buf, "\"E0: Sync\":3,"));
  TEST_ASSERT_NOT_NULL(strstr(buf, "\"63: Leaving water (C)\":33.00,"));
  TEST_ASSERT_NOT_NULL(strstr(buf, "\"RAM: Outdoor input current (A)\":4.40,"));
  TEST_ASSERT_NOT_NULL(strstr(buf, "\"RAM: Outdoor rated current (A)\":27.0,"));
  TEST_ASSERT_NOT_NULL(strstr(buf, "\"RAM: Outdoor load (%)\":16,"));
  reader.readCurrent = false;
  buf[0] = 0;
  reader.appendJson(buf, sizeof(buf));
  TEST_ASSERT_NULL(strstr(buf, "RAM:"));
}

// another system id and no BRC1 thermistor: the 63 water values still come, only the room stays null
void test_63_other_system_no_brc1()
{
  unit.id63 = 0x02;
  unit.room63 = 0;
  runFor(15 * 60000UL);
  TEST_ASSERT_TRUE(reader.r63Period > 0);
  TEST_ASSERT_FALSE(reader.r63Rejected);
  char buf[1024] = "";
  reader.appendJson(buf, sizeof(buf));
  TEST_ASSERT_NOT_NULL(strstr(buf, "\"63: Leaving water (C)\":33.00,"));
  TEST_ASSERT_NOT_NULL(strstr(buf, "\"63: Return water (C)\":28.50,"));
  TEST_ASSERT_NOT_NULL(strstr(buf, "\"63: BRC1 room temp (C)\":null,"));
}

// a 63 frame with an implausible water value is rejected, and logged once as hex
static int rejLines = 0;
static void countRej(const char *s)
{
  if (strstr(s, "63 rejected") || strstr(s, "63REJ"))
    rejLines++;
}
void test_63_reject_logged_once()
{
  rejLines = 0;
  reader.log = countRej;
  unit.badWater = true;
  runFor(15 * 60000UL);
  TEST_ASSERT_TRUE(reader.r63Rejected);
  TEST_ASSERT_EQUAL(2, rejLines); // the "rejected" line and one 63REJ hex line
  TEST_ASSERT_EQUAL(0, reader.r63Period);
  TEST_ASSERT_TRUE(reader.hydro.locked && reader.outdoor.locked); // E0 not affected
}

// the ERSQ016AV1 software: same slots, so the frames are read
void test_yp0211_is_read()
{
  unit.swId = "YP0211_004";
  runFor(15 * 60000UL);
  TEST_ASSERT_EQUAL(DIII_READY, reader.status);
  TEST_ASSERT_EQUAL_STRING("YP0211_004", reader.softwareId);
  TEST_ASSERT_TRUE(reader.hydro.locked && reader.outdoor.locked && reader.r63Period > 0);
}

// an unknown software id is kept for the log, nothing else is read
void test_unknown_id_is_reported()
{
  unit.swId = "ZQ9999_001";
  runFor(3 * 60000UL);
  TEST_ASSERT_EQUAL(DIII_UNSUPPORTED, reader.status);
  TEST_ASSERT_EQUAL_STRING("ZQ9999_001", reader.softwareId);
}

// other variables at 0x2B70 (rated 0 here): no current, logged once, the frames still read
static int curLines = 0;
static void countCur(const char *s)
{
  if (strstr(s, "RAM current not recognised") || strncmp(s, "CUR", 3) == 0)
    curLines++;
}
void test_current_checked()
{
  curLines = 0;
  reader.log = countCur;
  unit.rated = 0;
  runFor(15 * 60000UL);
  TEST_ASSERT_TRUE(reader.curRejected);
  TEST_ASSERT_EQUAL(2, curLines);
  TEST_ASSERT_EQUAL(0, reader.v.curAt);
  char buf[1024] = "";
  reader.appendJson(buf, sizeof(buf));
  TEST_ASSERT_NOT_NULL(strstr(buf, "\"RAM: Outdoor input current (A)\":null,"));
  TEST_ASSERT_NOT_NULL(strstr(buf, "\"E0: Leaving water (C)\":33,"));
}

// the DHW tank and the 3-way valve from the hydro E0
void test_dhw_tank_and_valve()
{
  runFor(15 * 60000UL);
  char buf[1024] = "";
  reader.appendJson(buf, sizeof(buf));
  TEST_ASSERT_NOT_NULL(strstr(buf, "\"E0: DHW tank (C)\":48,"));
  TEST_ASSERT_NOT_NULL(strstr(buf, "\"E0: 3-way valve DHW\":1,"));
}

// no tank sensor (-47) and the valve on space heating: tank null, valve 0
void test_no_tank_sensor()
{
  unit.tank = 0xAF;
  unit.bits0C = 0x42;
  runFor(15 * 60000UL);
  char buf[1024] = "";
  reader.appendJson(buf, sizeof(buf));
  TEST_ASSERT_NOT_NULL(strstr(buf, "\"E0: DHW tank (C)\":null,"));
  TEST_ASSERT_NOT_NULL(strstr(buf, "\"E0: 3-way valve DHW\":0,"));
  TEST_ASSERT_NOT_NULL(strstr(buf, "\"E0: Leaving water (C)\":33,"));
}

void test_values_go_null_when_frames_stop(){
  runFor(15 * 60000UL);
  unit.answer = false;
  runFor(4 * 60000UL);
  char buf[1024] = "";
  reader.appendJson(buf, sizeof(buf));
  TEST_ASSERT_NOT_NULL(strstr(buf, "\"E0: Leaving water (C)\":null,"));
  TEST_ASSERT_FALSE(reader.hydro.locked);
}

void test_window_hold_keeps_polls_off()
{
  runFor(15 * 60000UL);
  // just before the hydro window: hold() must tick through it and return once it is over
  unsigned long target = (fakeNow / 60000 + 1) * 60000 + 22000 - 3000;
  runFor(target - fakeNow);
  TEST_ASSERT_TRUE(reader.windowWithin(5000));
  reader.hold(5000, [](unsigned long ms) { fakeNow += ms; });
  TEST_ASSERT_FALSE(reader.windowWithin(1000));
  TEST_ASSERT_TRUE(fakeClock() - reader.v.hydroAt < 5000);
}

int main(int, char **)
{
  UNITY_BEGIN();
  RUN_TEST(test_silent_before_start_delay);
  RUN_TEST(test_other_software_reads_nothing_more);
  RUN_TEST(test_catches_all_frames_read_only);
  RUN_TEST(test_learns_despite_a_30s_poll);
  RUN_TEST(test_locked_traffic_is_low);
  RUN_TEST(test_json_keys);
  RUN_TEST(test_63_other_system_no_brc1);
  RUN_TEST(test_63_reject_logged_once);
  RUN_TEST(test_yp0211_is_read);
  RUN_TEST(test_unknown_id_is_reported);
  RUN_TEST(test_current_checked);
  RUN_TEST(test_dhw_tank_and_valve);
  RUN_TEST(test_no_tank_sensor);
  RUN_TEST(test_values_go_null_when_frames_stop);
  RUN_TEST(test_window_hold_keeps_polls_off);
  return UNITY_END();
}
