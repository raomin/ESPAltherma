// Event history ring (include/eventring.h).
#include <unity.h>
#include <stdio.h>
#include "eventring.h"

void setUp() {}
void tearDown() {}

void test_empty_ring_is_valid()
{
  EventRing r;
  r.clear();
  TEST_ASSERT_TRUE(r.valid());
  TEST_ASSERT_EQUAL_UINT32(0, r.count());
}

void test_random_memory_is_not_valid()
{
  EventRing r;
  memset(&r, 0xA5, sizeof(r));
  TEST_ASSERT_FALSE(r.valid());
  r.clear();
  r.add(0, 5, 1, "Boot #1: power on");
  r.entries[0].text[0] = 'X'; // one bit flipped by a brownout
  TEST_ASSERT_FALSE(r.valid());
}

void test_order_and_wraparound()
{
  EventRing r;
  r.clear();
  char text[16];
  for (int i = 0; i < EVENT_COUNT + 5; i++)
  {
    snprintf(text, sizeof(text), "event %d", i);
    r.add(0, i, 1, text);
  }
  TEST_ASSERT_TRUE(r.valid());
  TEST_ASSERT_EQUAL_UINT32(EVENT_COUNT, r.count());
  TEST_ASSERT_EQUAL_STRING("event 5", r.at(0).text); // the 5 oldest were overwritten
  snprintf(text, sizeof(text), "event %d", EVENT_COUNT + 4);
  TEST_ASSERT_EQUAL_STRING(text, r.at(EVENT_COUNT - 1).text);
}

void test_long_text_is_cut()
{
  EventRing r;
  r.clear();
  char text[200];
  memset(text, 'a', sizeof(text) - 1);
  text[sizeof(text) - 1] = 0;
  r.add(0, 1, 1, text);
  TEST_ASSERT_EQUAL_UINT32(EVENT_TEXT_SIZE - 1, strlen(r.at(0).text));
}

void test_backfill_dates_only_this_boot()
{
  EventRing r;
  r.clear();
  r.add(1760000000, 100, 7, "previous boot, dated");
  r.add(0, 2, 7, "previous boot, never dated");
  r.add(0, 1, 8, "Boot #8: power on");
  r.add(0, 9, 8, "WiFi: joined 20:23:51:97:82:12, channel 6");
  r.backfill(8, 1760003600, 20); // the clock is set 20s after boot
  TEST_ASSERT_TRUE(r.valid());
  TEST_ASSERT_EQUAL_UINT32(1760000000, r.at(0).epoch);
  TEST_ASSERT_EQUAL_UINT32(0, r.at(1).epoch);
  TEST_ASSERT_EQUAL_UINT32(1760003600 - 19, r.at(2).epoch);
  TEST_ASSERT_EQUAL_UINT32(1760003600 - 11, r.at(3).epoch);
}

int main(int, char **)
{
  UNITY_BEGIN();
  RUN_TEST(test_empty_ring_is_valid);
  RUN_TEST(test_random_memory_is_not_valid);
  RUN_TEST(test_order_and_wraparound);
  RUN_TEST(test_long_text_is_cut);
  RUN_TEST(test_backfill_dates_only_this_boot);
  return UNITY_END();
}
