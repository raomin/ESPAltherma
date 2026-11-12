#include <unity.h>
#include "expert.h"
void setUp() {}
void tearDown() {}

void test_hex_is_exact_and_bounded()
{
  uint8_t bytes[4]; size_t n;
  TEST_ASSERT_TRUE(expertHex("03 40\n60\t5c", bytes, 4, n));
  TEST_ASSERT_EQUAL(4, n);
  TEST_ASSERT_EQUAL_HEX8(0x5c, bytes[3]);
  TEST_ASSERT_FALSE(expertHex("03 40 60 5c 00", bytes, 4, n));
  TEST_ASSERT_FALSE(expertHex("03 4", bytes, 4, n));
  TEST_ASSERT_FALSE(expertHex("0x03", bytes, 4, n));
  TEST_ASSERT_FALSE(expertHex("", bytes, 4, n));
}

void test_explicit_enable_and_one_transaction()
{
  ExpertSession s; ExpertJob request, taken;
  request.tx[0] = 0x15; request.txLen = 1;
  TEST_ASSERT_FALSE(s.submit(request, 0));
  s.setEnabled(true, 1);
  TEST_ASSERT_TRUE(s.submit(request, 2));
  TEST_ASSERT_FALSE(s.submit(request, 3));
  TEST_ASSERT_TRUE(s.take(taken, 4));
  TEST_ASSERT_FALSE(s.submit(request, 5));
  // Even malformed replies remain available byte-for-byte.
  taken.rx[0] = 0x15; taken.rx[1] = 0xea; taken.rxLen = 2;
  s.finish(taken);
  TEST_ASSERT_TRUE(s.done);
  TEST_ASSERT_EQUAL_HEX8(0xea, s.job.rx[1]);
  TEST_ASSERT_TRUE(s.submit(request, 6));
  TEST_ASSERT_EQUAL(2, s.job.id);
  TEST_ASSERT_FALSE(s.done);
  TEST_ASSERT_EQUAL(0, s.job.rxLen);
}

void test_disable_cancels_pending_but_preserves_running_reply()
{
  ExpertSession s; ExpertJob request, taken;
  request.txLen = 1;
  s.setEnabled(true, 0); s.submit(request, 1);
  s.setEnabled(false, 2);
  TEST_ASSERT_FALSE(s.take(taken, 3));
  TEST_ASSERT_TRUE(s.job.cancelled);
  s.setEnabled(true, 4); s.submit(request, 5); s.take(taken, 6);
  s.setEnabled(false, 7);
  taken.rx[0] = 0x46; taken.rxLen = 1; s.finish(taken);
  TEST_ASSERT_FALSE(s.submit(request, 8));
  TEST_ASSERT_TRUE(s.done);
  TEST_ASSERT_EQUAL_HEX8(0x46, s.job.rx[0]);
}

void test_expiry_and_millis_wrap()
{
  ExpertSession s; ExpertJob request, taken; request.txLen = 1;
  uint32_t start = UINT32_MAX - 100;
  s.setEnabled(true, start);
  TEST_ASSERT_TRUE(s.active(start + 200));
  TEST_ASSERT_TRUE(s.submit(request, start + ExpertSession::LEASE_MS - 1));
  TEST_ASSERT_FALSE(s.take(taken, start + ExpertSession::LEASE_MS));
  TEST_ASSERT_TRUE(s.job.cancelled);
  TEST_ASSERT_FALSE(s.enabled);
}

void test_timing_and_frame_limits()
{
  ExpertSession s; ExpertJob j; s.setEnabled(true, 0);
  TEST_ASSERT_FALSE(s.submit(j, 1));
  j.txLen = ExpertJob::TX_MAX + 1;
  TEST_ASSERT_FALSE(s.submit(j, 1));
  j.txLen = 1; j.timeoutMs = 2001;
  TEST_ASSERT_FALSE(s.submit(j, 1));
  j.timeoutMs = 20; j.idleMs = 21;
  TEST_ASSERT_FALSE(s.submit(j, 1));
  j.idleMs = 4;
  TEST_ASSERT_FALSE(s.submit(j, 1));
  j.idleMs = 5;
  TEST_ASSERT_TRUE(s.submit(j, 1));
}

int main()
{
  UNITY_BEGIN();
  RUN_TEST(test_hex_is_exact_and_bounded);
  RUN_TEST(test_explicit_enable_and_one_transaction);
  RUN_TEST(test_disable_cancels_pending_but_preserves_running_reply);
  RUN_TEST(test_expiry_and_millis_wrap);
  RUN_TEST(test_timing_and_frame_limits);
  return UNITY_END();
}
