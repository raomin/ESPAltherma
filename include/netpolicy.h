#ifndef ESPALTHERMA_NETPOLICY_H
#define ESPALTHERMA_NETPOLICY_H

// Connection policy for WiFi and Ethernet: when to reconnect, move to a stronger access point, start or stop
// the setup access point, and reboot. The device is "online" when either link has an IP.
// Pure logic (the caller passes the time and the link states), so the native tests replay outages;
// netmgr.h applies the actions.
//
//  - WiFi: the core's auto-reconnect gets 15s, then a full scan every 15s picks the strongest access point.
//  - An outage ends only after 2 minutes online: a link that keeps dropping does not reset the timers.
//  - Offline for 2 minutes: setup access point. Offline for 5 minutes (30 when never online since boot, eg.
//    wrong credentials) and nobody on the setup access point: reboot.
//  - Online but the MQTT broker unreachable: over WiFi, reconnect to the strongest access point after 2 minutes
//    (the link may be stuck on a distant one), then every 5 minutes; reboot after 15 minutes.
//  - Ethernet, when the board has it, is preferred: WiFi (if configured) is only used while Ethernet is down.
//  - Several access points with the same SSID: while the signal is weak, scan for one at least 8dB stronger.

#include <stdint.h>

#define NET_WIFI_RETRY_MS 15000UL
#define NET_AP_FALLBACK_MS 120000UL
#define NET_AP_IDLE_STOP_MS 300000UL
#define NET_STABLE_MS 120000UL
#define NET_REBOOT_MS (5UL * 60 * 1000)
#define NET_REBOOT_FIRST_MS (30UL * 60 * 1000)
#define NET_MQTT_RESCAN_MS 120000UL
#define NET_MQTT_RESCAN_EVERY_MS 300000UL
#define NET_MQTT_REBOOT_MS (15UL * 60 * 1000)
#define NET_ETH_FAILOVER_MS 30000UL
#define NET_ETH_PREFER_MS 60000UL
#define NET_ROAM_WEAK_RSSI -75 // weaker: look for a stronger access point every minute
#define NET_ROAM_RSSI -67      // weaker: every 5 minutes
#define NET_ROAM_WEAK_EVERY_MS 60000UL
#define NET_ROAM_EVERY_MS 300000UL
#define NET_ROAM_GAIN 8 // dB

enum NetAction : uint8_t
{
  NET_CONNECT_WIFI = 1, // full scan, then the strongest access point of the SSID
  NET_STOP_WIFI = 2,
  NET_START_AP = 4,
  NET_STOP_AP = 8,
  NET_ROAM_SCAN = 16, // scan for a stronger access point of the same SSID (see netRoamPick)
  NET_REBOOT = 32,
};

struct NetInputs
{
  unsigned long now = 0;
  bool wifiConfigured = false; // an SSID is set
  bool wifiUp = false;         // station connected
  int rssi = 0;                // of the station, while connected
  bool ethEnabled = false;     // the board has Ethernet
  bool ethUp = false;          // Ethernet link with an IP
  bool apActive = false;
  int apClients = 0;
  bool mqttConfigured = false;
  bool mqttUp = false;
};

struct NetPolicy
{
  const char *reason = nullptr; // why the actions of the last step were taken (log, restart cause)

  void begin(unsigned long now)
  {
    *this = NetPolicy();
    offlineSince = now;
    ethChangedAt = now;
  }

  // The caller connected the WiFi itself (new credentials): no rescan for NET_WIFI_RETRY_MS.
  void noteWifiAttempt(unsigned long now)
  {
    wifiAttempted = true;
    lastWifiAttempt = now;
  }

  bool online() const { return linkWasUp; }
  bool everOnline() const { return wasOnline; }

  uint8_t step(const NetInputs &in)
  {
    uint8_t act = 0;
    reason = nullptr;
    const unsigned long now = in.now;
    const bool linkUp = in.wifiUp || in.ethUp;

    if (in.ethUp != ethWasUp)
    {
      ethWasUp = in.ethUp;
      ethChangedAt = now;
    }
    if (in.wifiUp != wifiWasUp)
    {
      wifiWasUp = in.wifiUp;
      if (in.wifiUp)
        lastRoamScan = now; // just picked the strongest access point
      else
        lastWifiAttempt = now; // the core's auto-reconnect goes first
    }
    if (linkUp != linkWasUp)
    {
      linkWasUp = linkUp;
      if (linkUp)
        onlineSince = now;
    }
    if (linkUp)
    {
      wasOnline = true;
      if (outage && now - onlineSince >= NET_STABLE_MS)
        outage = false;
    }
    else if (!outage)
    {
      outage = true;
      offlineSince = now;
    }

    // WiFi station: Ethernet goes first when the board has it
    const bool ethPreferred = in.ethEnabled && in.ethUp && now - ethChangedAt >= NET_ETH_PREFER_MS;
    const bool wifiWanted = in.wifiConfigured && !(in.ethEnabled && (in.ethUp || now - ethChangedAt < NET_ETH_FAILOVER_MS));
    if (in.wifiUp && ethPreferred)
    {
      act |= NET_STOP_WIFI;
      reason = "Ethernet is up: WiFi stopped";
    }
    else if (wifiWanted && !in.wifiUp && (!wifiAttempted || now - lastWifiAttempt >= NET_WIFI_RETRY_MS))
    {
      act |= NET_CONNECT_WIFI;
      noteWifiAttempt(now);
    }

    // Setup access point
    const bool networkConfigured = in.wifiConfigured || in.ethEnabled;
    if (!in.apActive)
    {
      if (!networkConfigured)
      {
        act |= NET_START_AP;
        reason = "No network configured";
      }
      else if (!linkUp && now - offlineSince >= NET_AP_FALLBACK_MS)
      {
        act |= NET_START_AP;
        reason = "No network for 2 minutes";
      }
    }
    else if (linkUp && now - onlineSince >= NET_AP_IDLE_STOP_MS && in.apClients == 0)
    {
      act |= NET_STOP_AP;
    }

    // Reboot when offline, in case the network stack is wedged. Never while someone uses the setup access point.
    const unsigned long offlineLimit = wasOnline ? NET_REBOOT_MS : NET_REBOOT_FIRST_MS;
    if (networkConfigured && !linkUp && now - offlineSince >= offlineLimit && in.apClients == 0)
    {
      reason = wasOnline ? "No network for 5 minutes" : "No network for 30 minutes since boot";
      return act | NET_REBOOT;
    }

    // Online, but the broker does not answer
    if (in.mqttConfigured && linkUp && !in.mqttUp && !mqttDown)
    {
      mqttDown = true;
      mqttDownSince = now;
      mqttRescanned = false;
    }
    else if (!in.mqttConfigured || in.mqttUp)
    {
      mqttDown = false;
    }
    if (mqttDown && linkUp)
    {
      if (now - mqttDownSince >= NET_MQTT_REBOOT_MS && in.apClients == 0)
      {
        reason = "MQTT broker unreachable for 15 minutes";
        return act | NET_REBOOT;
      }
      if (in.wifiUp && !in.ethUp && now - mqttDownSince >= NET_MQTT_RESCAN_MS &&
          (!mqttRescanned || now - lastMqttRescan >= NET_MQTT_RESCAN_EVERY_MS))
      {
        mqttRescanned = true;
        lastMqttRescan = now;
        noteWifiAttempt(now);
        act |= NET_CONNECT_WIFI;
        reason = "MQTT broker unreachable: reconnecting to the strongest access point";
      }
    }

    // Weak signal: is a stronger access point of the same SSID around?
    if (in.wifiUp && !in.ethUp && !in.apActive && in.rssi != 0 && in.rssi < NET_ROAM_RSSI && !(act & (NET_CONNECT_WIFI | NET_STOP_WIFI)))
    {
      const unsigned long every = in.rssi < NET_ROAM_WEAK_RSSI ? NET_ROAM_WEAK_EVERY_MS : NET_ROAM_EVERY_MS;
      if (now - lastRoamScan >= every)
      {
        lastRoamScan = now;
        act |= NET_ROAM_SCAN;
      }
    }
    return act;
  }

private:
  bool linkWasUp = false;
  bool wifiWasUp = false;
  bool ethWasUp = false;
  bool wasOnline = false;
  bool outage = true; // from boot until online for NET_STABLE_MS
  unsigned long offlineSince = 0;
  unsigned long onlineSince = 0;
  unsigned long ethChangedAt = 0;
  bool wifiAttempted = false;
  unsigned long lastWifiAttempt = 0;
  bool mqttDown = false;
  unsigned long mqttDownSince = 0;
  bool mqttRescanned = false;
  unsigned long lastMqttRescan = 0;
  unsigned long lastRoamScan = 0;
};

// An access point of our SSID found by a scan
struct NetAp
{
  uint8_t bssid[6];
  int rssi;
  int channel;
};

// Index of the access point to move to, or -1 to stay: the strongest other one, at least `gain` dB stronger.
int netRoamPick(const NetAp *aps, int count, const uint8_t *currentBssid, int currentRssi, int gain = NET_ROAM_GAIN)
{
  int best = -1;
  for (int i = 0; i < count; i++)
  {
    bool same = true;
    for (int b = 0; b < 6; b++)
      same = same && aps[i].bssid[b] == currentBssid[b];
    if (!same && aps[i].rssi >= currentRssi + gain && (best < 0 || aps[i].rssi > aps[best].rssi))
      best = i;
  }
  return best;
}

#endif
