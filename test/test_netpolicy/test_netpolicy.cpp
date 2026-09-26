// Connection policy (include/netpolicy.h): outages replayed second by second.
#include <unity.h>
#include "netpolicy.h"

void setUp() {}
void tearDown() {}

static const unsigned long S = 1000UL;
static const unsigned long MIN = 60 * S;

struct Sim
{
  NetPolicy policy;
  NetInputs in;
  unsigned long t = 0;

  Sim(bool wifi, bool eth = false, bool mqtt = false)
  {
    in.wifiConfigured = wifi;
    in.ethEnabled = eth;
    in.mqttConfigured = mqtt;
    policy.begin(0);
  }

  // Steps once per second for `duration`; returns the actions seen, and when the first `watch` action happened.
  uint8_t run(unsigned long duration, uint8_t watch = 0, unsigned long *firstAt = nullptr)
  {
    uint8_t seen = 0;
    for (unsigned long end = t + duration; t < end; t += S)
    {
      in.now = t;
      uint8_t act = policy.step(in);
      if (firstAt && (act & watch) && !(seen & watch))
        *firstAt = t;
      seen |= act;
      if (act & NET_START_AP)
        in.apActive = true;
      if (act & NET_STOP_AP)
        in.apActive = false;
      if (act & NET_STOP_WIFI)
        in.wifiUp = false;
    }
    return seen;
  }
};

void test_fresh_device_starts_access_point_and_never_reboots()
{
  Sim s(false);
  unsigned long at = 0;
  uint8_t seen = s.run(2 * 60 * MIN, NET_START_AP, &at);
  TEST_ASSERT_TRUE(seen & NET_START_AP);
  TEST_ASSERT_EQUAL_UINT32(0, at);
  TEST_ASSERT_FALSE(seen & (NET_REBOOT | NET_CONNECT_WIFI));
}

void test_wifi_connects_at_boot_then_rescans_every_15s_after_a_loss()
{
  Sim s(true);
  unsigned long at = 99;
  TEST_ASSERT_TRUE(s.run(S, NET_CONNECT_WIFI, &at) & NET_CONNECT_WIFI);
  TEST_ASSERT_EQUAL_UINT32(0, at);
  s.in.wifiUp = true;
  TEST_ASSERT_EQUAL_UINT8(0, s.run(10 * MIN));

  s.in.wifiUp = false; // lost at 10:01
  unsigned long lost = s.t;
  TEST_ASSERT_FALSE(s.run(14 * S) & NET_CONNECT_WIFI); // the core's auto-reconnect goes first
  s.run(2 * S, NET_CONNECT_WIFI, &at);
  TEST_ASSERT_EQUAL_UINT32(lost + 15 * S, at);
}

void test_outage_starts_access_point_at_2_minutes_and_reboots_at_5()
{
  Sim s(true);
  s.in.wifiUp = true;
  s.run(10 * MIN);
  s.in.wifiUp = false;
  unsigned long lost = s.t, apAt = 0, rebootAt = 0;
  s.run(2 * MIN + S, NET_START_AP, &apAt);
  TEST_ASSERT_EQUAL_UINT32(lost + 2 * MIN, apAt);
  s.run(4 * MIN, NET_REBOOT, &rebootAt);
  TEST_ASSERT_EQUAL_UINT32(lost + 5 * MIN, rebootAt);
  TEST_ASSERT_EQUAL_STRING("No network for 5 minutes", s.policy.reason);
}

void test_no_reboot_while_someone_is_on_the_setup_access_point()
{
  Sim s(true);
  s.in.wifiUp = true;
  s.run(10 * MIN);
  s.in.wifiUp = false;
  s.run(3 * MIN);
  s.in.apClients = 1;
  TEST_ASSERT_FALSE(s.run(60 * MIN) & NET_REBOOT);
}

void test_never_online_since_boot_waits_30_minutes()
{
  Sim s(true); // eg. wrong password
  unsigned long at = 0;
  s.run(31 * MIN, NET_REBOOT, &at);
  TEST_ASSERT_EQUAL_UINT32(30 * MIN, at);
}

void test_a_flapping_link_does_not_reset_the_outage()
{
  Sim s(true);
  s.in.wifiUp = true;
  s.run(10 * MIN);
  unsigned long lost = s.t, at = 0;
  uint8_t seen = 0;
  for (int i = 0; i < 10 && !(seen & NET_REBOOT); i++)
  { // 30s up, 30s down: the weak access point
    s.in.wifiUp = true;
    seen |= s.run(30 * S, NET_REBOOT, &at);
    s.in.wifiUp = false;
    seen |= s.run(30 * S, NET_REBOOT, &at);
  }
  TEST_ASSERT_TRUE(seen & NET_REBOOT);
  TEST_ASSERT_UINT32_WITHIN(30 * S, lost + 5 * MIN + 30 * S, at);
}

void test_two_stable_minutes_end_the_outage()
{
  Sim s(true);
  s.in.wifiUp = true;
  s.run(10 * MIN);
  s.in.wifiUp = false;
  s.run(4 * MIN);
  s.in.wifiUp = true;
  s.run(2 * MIN + S);
  s.in.wifiUp = false;
  unsigned long lost = s.t, at = 0;
  s.run(6 * MIN, NET_REBOOT, &at);
  TEST_ASSERT_EQUAL_UINT32(lost + 5 * MIN, at);
}

void test_access_point_stops_after_5_minutes_online_without_clients()
{
  Sim s(true);
  s.run(3 * MIN); // offline: access point up
  TEST_ASSERT_TRUE(s.in.apActive);
  s.in.wifiUp = true;
  unsigned long up = s.t, at = 0;
  s.run(6 * MIN, NET_STOP_AP, &at);
  TEST_ASSERT_FALSE(s.in.apActive);
  TEST_ASSERT_EQUAL_UINT32(up + 5 * MIN, at);
}

void test_broker_unreachable_over_wifi_rescans_then_reboots()
{
  Sim s(true, false, true);
  s.in.wifiUp = true;
  s.in.mqttUp = true;
  s.run(10 * MIN);
  s.in.mqttUp = false; // stuck on an access point that passes nothing
  unsigned long down = s.t, at = 0;
  s.run(2 * MIN + S, NET_CONNECT_WIFI, &at);
  TEST_ASSERT_EQUAL_UINT32(down + 2 * MIN, at);
  TEST_ASSERT_FALSE(s.run(4 * MIN) & NET_CONNECT_WIFI);
  s.run(2 * MIN, NET_CONNECT_WIFI, &at);
  TEST_ASSERT_EQUAL_UINT32(down + 7 * MIN, at);
  s.run(9 * MIN, NET_REBOOT, &at);
  TEST_ASSERT_EQUAL_UINT32(down + 15 * MIN, at);
  TEST_ASSERT_EQUAL_STRING("MQTT broker unreachable for 15 minutes", s.policy.reason);
}

void test_broker_back_resets_the_count()
{
  Sim s(true, false, true);
  s.in.wifiUp = true;
  s.in.mqttUp = false;
  s.run(10 * MIN);
  s.in.mqttUp = true;
  s.run(S);
  s.in.mqttUp = false;
  TEST_ASSERT_FALSE(s.run(14 * MIN) & NET_REBOOT);
}

void test_ethernet_only_board_needs_no_wifi_nor_access_point()
{
  Sim s(false, true, true);
  s.run(5 * S);
  s.in.ethUp = true;
  s.in.mqttUp = true;
  uint8_t seen = s.run(2 * 60 * MIN);
  TEST_ASSERT_FALSE(seen & (NET_CONNECT_WIFI | NET_START_AP | NET_REBOOT | NET_ROAM_SCAN));
}

void test_ethernet_cable_unplugged()
{
  Sim s(false, true);
  s.in.ethUp = true;
  s.run(10 * MIN);
  s.in.ethUp = false;
  unsigned long lost = s.t, apAt = 0, rebootAt = 0;
  uint8_t seen = s.run(2 * MIN + S, NET_START_AP, &apAt);
  TEST_ASSERT_FALSE(seen & NET_CONNECT_WIFI);
  TEST_ASSERT_EQUAL_UINT32(lost + 2 * MIN, apAt);
  s.run(4 * MIN, NET_REBOOT, &rebootAt);
  TEST_ASSERT_EQUAL_UINT32(lost + 5 * MIN, rebootAt);
}

void test_broker_unreachable_over_ethernet_does_not_touch_wifi()
{
  Sim s(true, true, true);
  s.in.ethUp = true;
  s.run(10 * MIN);
  unsigned long at = 0;
  uint8_t seen = s.run(16 * MIN, NET_REBOOT, &at);
  TEST_ASSERT_FALSE(seen & NET_CONNECT_WIFI);
  TEST_ASSERT_EQUAL_UINT32(15 * MIN, at); // broker down since Ethernet came up at 0
}

void test_wifi_backs_up_ethernet()
{
  Sim s(true, true);
  s.run(10 * S);
  s.in.ethUp = true; // Ethernet within 30s of boot: WiFi never started
  TEST_ASSERT_FALSE(s.run(10 * MIN) & NET_CONNECT_WIFI);

  s.in.ethUp = false;
  unsigned long lost = s.t, at = 0;
  s.run(31 * S, NET_CONNECT_WIFI, &at);
  TEST_ASSERT_EQUAL_UINT32(lost + 30 * S, at);
  s.in.wifiUp = true;
  TEST_ASSERT_FALSE(s.run(10 * MIN) & (NET_START_AP | NET_REBOOT)); // online over WiFi

  s.in.ethUp = true;
  unsigned long back = s.t;
  s.run(61 * S, NET_STOP_WIFI, &at);
  TEST_ASSERT_EQUAL_UINT32(back + 60 * S, at);
  TEST_ASSERT_FALSE(s.run(10 * MIN) & NET_CONNECT_WIFI);
}

void test_ethernet_board_without_ethernet_at_boot_falls_back_to_wifi()
{
  Sim s(true, true);
  unsigned long at = 0;
  s.run(31 * S, NET_CONNECT_WIFI, &at);
  TEST_ASSERT_EQUAL_UINT32(30 * S, at);
}

void test_roam_scans_only_while_weak()
{
  Sim s(true);
  s.in.wifiUp = true;
  s.in.rssi = -50;
  TEST_ASSERT_FALSE(s.run(30 * MIN) & NET_ROAM_SCAN);

  s.in.rssi = -70; // every 5 minutes
  unsigned long at = 0;
  s.run(S, NET_ROAM_SCAN, &at); // connected for a while already: right away
  TEST_ASSERT_EQUAL_UINT32(30 * MIN, at);
  s.run(5 * MIN, NET_ROAM_SCAN, &at);
  TEST_ASSERT_EQUAL_UINT32(35 * MIN, at);

  s.in.rssi = -80; // every minute
  s.run(MIN, NET_ROAM_SCAN, &at);
  at = 0;
  s.run(MIN, NET_ROAM_SCAN, &at);
  TEST_ASSERT_TRUE(at > 0);

  s.in.apActive = true; // a scan would disturb the setup access point (someone on it, so it stays up)
  s.in.apClients = 1;
  TEST_ASSERT_FALSE(s.run(10 * MIN) & NET_ROAM_SCAN);
}

void test_roam_pick()
{
  const uint8_t current[6] = {0x20, 0x23, 0x51, 0x97, 0x82, 0x12};
  NetAp aps[] = {
      {{0x20, 0x23, 0x51, 0x97, 0x82, 0x12}, -60, 6},  // ourselves, from the scan
      {{0x6a, 0x48, 0xb8, 0xea, 0x44, 0x08}, -55, 9},  // only 5dB better than the current -60... see below
      {{0x80, 0x3f, 0x5d, 0x67, 0xa1, 0x91}, -48, 12}, // the strongest
  };
  TEST_ASSERT_EQUAL_INT(2, netRoamPick(aps, 3, current, -78));
  TEST_ASSERT_EQUAL_INT(-1, netRoamPick(aps, 3, current, -42)); // nothing 8dB stronger
  TEST_ASSERT_EQUAL_INT(2, netRoamPick(aps, 3, current, -56));  // -48 is 8dB better, -55 is not
  TEST_ASSERT_EQUAL_INT(-1, netRoamPick(aps, 1, current, -90)); // only ourselves
}

int main(int, char **)
{
  UNITY_BEGIN();
  RUN_TEST(test_fresh_device_starts_access_point_and_never_reboots);
  RUN_TEST(test_wifi_connects_at_boot_then_rescans_every_15s_after_a_loss);
  RUN_TEST(test_outage_starts_access_point_at_2_minutes_and_reboots_at_5);
  RUN_TEST(test_no_reboot_while_someone_is_on_the_setup_access_point);
  RUN_TEST(test_never_online_since_boot_waits_30_minutes);
  RUN_TEST(test_a_flapping_link_does_not_reset_the_outage);
  RUN_TEST(test_two_stable_minutes_end_the_outage);
  RUN_TEST(test_access_point_stops_after_5_minutes_online_without_clients);
  RUN_TEST(test_broker_unreachable_over_wifi_rescans_then_reboots);
  RUN_TEST(test_broker_back_resets_the_count);
  RUN_TEST(test_ethernet_only_board_needs_no_wifi_nor_access_point);
  RUN_TEST(test_ethernet_cable_unplugged);
  RUN_TEST(test_broker_unreachable_over_ethernet_does_not_touch_wifi);
  RUN_TEST(test_wifi_backs_up_ethernet);
  RUN_TEST(test_ethernet_board_without_ethernet_at_boot_falls_back_to_wifi);
  RUN_TEST(test_roam_scans_only_while_weak);
  RUN_TEST(test_roam_pick);
  return UNITY_END();
}
