#ifndef ESPALTHERMA_NETMGR_H
#define ESPALTHERMA_NETMGR_H

// Network manager (ESP32): never blocks the main loop. When to reconnect, roam, start the setup access point
// or reboot is decided in netpolicy.h; this file applies it.
//  - WiFi station, and wired Ethernet on boards built with HAS_ETHERNET (preferred when both work).
//  - Without a network for 2 minutes (or without any network configured), an access point "ESPAltherma-XXXX"
//    is started with a captive portal serving the web interface.
//  - mDNS (espaltherma.local) and ArduinoOTA once online.
//  - MQTT: one connection attempt every 5s, when configured.
//  - Discovery of Home Assistant / MQTT brokers on the network (mDNS), for the setup wizard.

#include <WiFi.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#ifdef HAS_ETHERNET
#include <ETH.h>
#endif
#include <esp_ota_ops.h>
#include "netpolicy.h"
#include "restart.h"

#define AP_PASSWORD "espaltherma" // WPA2 key of the setup access point (documented, shown on screen/serial)
#define MQTT_RETRY_MS 5000UL
#define ROAM_SCAN_MAX 16

// Defined in main.cpp
void wakeScreen();

DNSServer dnsServer;
NetPolicy netPolicy;
bool apActive = false;
char apSsid[33] = "";
bool netWasOnline = false;
bool networkServicesStarted = false;
unsigned long mqttLastAttempt = 0;
// MQTT status for the web interface: PubSubClient must only be used from the main loop
volatile bool mqttConnectedCache = false;
volatile int mqttStateCache = MQTT_DISCONNECTED;
volatile bool ethUp = false;

// Last access point joined and last disconnection: status page and restart cause
char wifiBssid[18] = "";
volatile int wifiChannel = 0;
int wifiRssi = 0; // last reading while connected
volatile uint8_t wifiLastDisconnect = 0;
uint8_t wifiLoggedReason = 0;
unsigned long wifiLoggedAt = 0;
unsigned wifiUnloggedCount = 0;
uint8_t wifiHistoryReason = 0; // the history gets reason changes, and repeats every 10 minutes
unsigned long wifiHistoryAt = 0;
unsigned wifiHistoryCount = 0;
bool roamScanning = false;

// Logged, and kept in the event history (eventlog.h) for postmortems. Any task.
static void logEvent(const char *format, ...)
{
  char text[EVENT_TEXT_SIZE];
  va_list args;
  va_start(args, format);
  vsnprintf(text, sizeof(text), format, args);
  va_end(args);
  mqttSerial.println(text);
  eventAdd(text);
}

// Network link changes (joined, lost, disconnected...): logged, and kept in the event history within a budget.
// A link that keeps dropping would otherwise push the start of the incident out of the history: up to 6
// entries, then one every 2 minutes; the next entry recorded tells how many were skipped. Any task.
#define NET_HISTORY_BURST 6
#define NET_HISTORY_EVERY_MS 120000UL
portMUX_TYPE netHistoryMux = portMUX_INITIALIZER_UNLOCKED;
int netHistoryTokens = NET_HISTORY_BURST;
unsigned long netHistoryRefill = 0;
unsigned netHistorySkipped = 0;

static void netHistory(bool log, const char *format, ...)
{
  char text[EVENT_TEXT_SIZE];
  va_list args;
  va_start(args, format);
  vsnprintf(text, sizeof(text), format, args);
  va_end(args);
  if (log)
    mqttSerial.println(text);
  unsigned long now = millis();
  unsigned skipped = 0;
  bool record;
  portENTER_CRITICAL(&netHistoryMux);
  if (netHistoryTokens >= NET_HISTORY_BURST)
    netHistoryRefill = now;
  while (netHistoryTokens < NET_HISTORY_BURST && now - netHistoryRefill >= NET_HISTORY_EVERY_MS)
  {
    netHistoryTokens++;
    netHistoryRefill += NET_HISTORY_EVERY_MS;
  }
  record = netHistoryTokens > 0;
  if (record)
  {
    netHistoryTokens--;
    skipped = netHistorySkipped;
    netHistorySkipped = 0;
  }
  else
  {
    netHistorySkipped++;
  }
  portEXIT_CRITICAL(&netHistoryMux);
  if (!record)
    return;
  if (skipped > 0)
    eventAddf("%s (+%u network changes not recorded)", text, skipped);
  else
    eventAdd(text);
}

bool netOnline() { return ethUp || WiFi.status() == WL_CONNECTED; }

IPAddress netIP()
{
#ifdef HAS_ETHERNET
  if (ethUp)
    return ETH.localIP();
#endif
  return WiFi.localIP();
}

const char *netLinkName()
{
  if (ethUp)
    return "ethernet";
  return WiFi.status() == WL_CONNECTED ? "wifi" : "none";
}

const char *wifiDisconnectName()
{
  return wifiLastDisconnect ? WiFi.disconnectReasonName((wifi_err_reason_t)wifiLastDisconnect) : "";
}

// Runs in the network event task: only logs (thread-safe) and sets flags.
static void netEvent(arduino_event_t *e)
{
  switch (e->event_id)
  {
  case ARDUINO_EVENT_WIFI_STA_CONNECTED:
  {
    const uint8_t *b = e->event_info.wifi_sta_connected.bssid;
    snprintf(wifiBssid, sizeof(wifiBssid), "%02X:%02X:%02X:%02X:%02X:%02X", b[0], b[1], b[2], b[3], b[4], b[5]);
    wifiChannel = e->event_info.wifi_sta_connected.channel;
    netHistory(true, "WiFi: joined access point %s, channel %d", wifiBssid, wifiChannel);
    wifiHistoryReason = 0;
    break;
  }
  case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
  {
    // The core's auto-reconnect fails every few seconds during an outage: log changes, and repeats once a minute
    uint8_t reason = e->event_info.wifi_sta_disconnected.reason;
    wifiLastDisconnect = reason;
    unsigned long now = millis();
    if (reason != wifiLoggedReason || now - wifiLoggedAt >= 60000UL)
    {
      mqttSerial.printf("WiFi: disconnected, %s (%u)", wifiDisconnectName(), reason);
      if (wifiUnloggedCount > 0)
        mqttSerial.printf(", %u more attempts failed", wifiUnloggedCount);
      mqttSerial.println();
      wifiLoggedReason = reason;
      wifiLoggedAt = now;
      wifiUnloggedCount = 0;
    }
    else
    {
      wifiUnloggedCount++;
    }
    if (reason != wifiHistoryReason || now - wifiHistoryAt >= 600000UL)
    {
      if (wifiHistoryCount > 0)
        netHistory(false, "WiFi: disconnected, %s (%u), %u more attempts failed", wifiDisconnectName(), reason, wifiHistoryCount);
      else
        netHistory(false, "WiFi: disconnected, %s (%u)", wifiDisconnectName(), reason);
      wifiHistoryReason = reason;
      wifiHistoryAt = now;
      wifiHistoryCount = 0;
    }
    else
    {
      wifiHistoryCount++;
    }
    break;
  }
#ifdef HAS_ETHERNET
  case ARDUINO_EVENT_ETH_START:
    ETH.setHostname(config.hostname);
    break;
  case ARDUINO_EVENT_ETH_CONNECTED:
    netHistory(true, "Ethernet: link up");
    break;
  case ARDUINO_EVENT_ETH_GOT_IP:
    ethUp = true;
    netHistory(true, "Ethernet: %s, %d Mbps%s", ETH.localIP().toString().c_str(), ETH.linkSpeed(), ETH.fullDuplex() ? " full duplex" : "");
    break;
  case ARDUINO_EVENT_ETH_DISCONNECTED:
    ethUp = false;
    netHistory(true, "Ethernet: link down");
    break;
  case ARDUINO_EVENT_ETH_STOP:
    ethUp = false;
    break;
#endif
  default:
    break;
  }
}

void startAccessPoint()
{
  if (apActive)
    return;
  uint8_t mac[6];
  WiFi.macAddress(mac);
  snprintf(apSsid, sizeof(apSsid), "ESPAltherma-%02X%02X", mac[4], mac[5]);
  WiFi.enableAP(true); // keeps the station (if any) running
  WiFi.softAP(apSsid, AP_PASSWORD);
  dnsServer.start(53, "*", WiFi.softAPIP());
  apActive = true;
  wakeScreen();
  mqttSerial.printf("Setup access point: %s (password: %s), open http://%s\n", apSsid, AP_PASSWORD, WiFi.softAPIP().toString().c_str());
  eventAddf("Setup access point %s started", apSsid);
}

void stopAccessPoint()
{
  if (!apActive)
    return;
  dnsServer.stop();
  WiFi.softAPdisconnect(true); // the access point only
  apActive = false;
  logEvent("Setup access point stopped");
}

// (Re)connects the station with the configured credentials: full scan, then the strongest access point.
void connectWifi()
{
  netPolicy.noteWifiAttempt(millis());
  if (!config.wifiSsid[0])
    return;
  mqttSerial.printf("Connecting to %s\n", config.wifiSsid);
  WiFi.enableSTA(true);
  WiFi.disconnect(false, false); // station only, keeps the access point
  if (config.staticIp)
  {
    if (!WiFi.config(IPAddress(config.ip), IPAddress(config.gateway), IPAddress(config.subnet), IPAddress(config.dns1), IPAddress(config.dns2)))
    {
      mqttSerial.println("Failed to set static ip!");
    }
  }
  WiFi.begin(config.wifiSsid, config.wifiPwd, 0, 0, true);
}

// Ethernet took over: the station is not needed (the radio stays on for the setup access point).
void stopWifi()
{
  WiFi.disconnect(!apActive, false);
}

// Arduino core (WiFiGeneric.cpp, also declared by its Ethernet library): starts the TCP/IP stack and the
// network event task. Idempotent.
#if ESP_ARDUINO_VERSION_MAJOR >= 3
#include <NetworkManager.h> // core 3.x: Network.begin() does it
#else
extern bool tcpipInit();
#endif

void netBegin()
{
  // The web server (webBegin, right after) needs the TCP/IP stack: AsyncTCP crashes at boot without it.
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  Network.begin();
#else
  tcpipInit();
#endif
  WiFi.persistent(false);
  WiFi.onEvent(netEvent);
  WiFi.setHostname(config.hostname);
  WiFi.setAutoReconnect(true);
  WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);
  WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
  netPolicy.begin(millis());
#ifdef HAS_ETHERNET
  if (!ETH.begin())
  {
    mqttSerial.println("Ethernet: no PHY found");
  }
  else if (config.staticIp)
  {
    ETH.config(IPAddress(config.ip), IPAddress(config.gateway), IPAddress(config.subnet), IPAddress(config.dns1), IPAddress(config.dns2));
  }
  // WiFi (backup) and the setup access point: when the policy decides
#else
  if (config.wifiSsid[0])
    connectWifi();
  else
    startAccessPoint();
#endif
}

// Firmware updates are kept only once proven. The core leaves a new image pending (verifyRollbackLater),
// and it is confirmed after 30s online (or with someone on the setup access point). If the new firmware
// crashes or restarts before, the bootloader starts the previous one again.
extern "C" bool verifyRollbackLater() { return true; }
bool otaChecked = false;
bool otaProving = false;
unsigned long otaProvingSince = 0;

static void otaConfirmLoop(unsigned long now, bool online)
{
  if (otaChecked)
    return;
  if (!online && !(apActive && WiFi.softAPgetStationNum() > 0))
  {
    otaProving = false;
    return;
  }
  if (!otaProving)
  {
    otaProving = true;
    otaProvingSince = now;
  }
  if (now - otaProvingSince < 30000UL)
    return;
  otaChecked = true;
  esp_ota_img_states_t state;
  if (esp_ota_get_state_partition(esp_ota_get_running_partition(), &state) == ESP_OK && state == ESP_OTA_IMG_PENDING_VERIFY)
  {
    esp_ota_mark_app_valid_cancel_rollback();
    logEvent("Firmware update confirmed: it will be kept");
  }
}

// OTA and mDNS, started when the device first gets online.
void startNetworkServices()
{
  if (networkServicesStarted)
    return;
  networkServicesStarted = true;
  ArduinoOTA.setHostname(config.hostname);
  if (config.adminPwd[0])
    ArduinoOTA.setPassword(config.adminPwd);
  ArduinoOTA.begin(); // also starts mDNS
  MDNS.addService("http", "tcp", 80);
  configTime(0, 0, "pool.ntp.org", "time.google.com"); // UTC, dates the event history
}

void mqttLoop()
{
  if (mqttWriteFailed)
  { // not client.disconnect(): its DISCONNECT packet would wait 10s more
    mqttWriteFailed = false;
    if (client.connected())
    {
      mqttNet->stop();
      netHistory(true, "MQTT: the broker stopped taking data, reconnecting");
    }
  }
  if (!config.mqttServer[0] || client.connected())
    return;
  unsigned long now = millis();
  if (mqttLastAttempt != 0 && now - mqttLastAttempt < MQTT_RETRY_MS)
    return;
  mqttLastAttempt = now;
  LOOP_STAGE("MQTT connect");
  mqttConnectOnce(); // netPolicy reboots when the broker stays unreachable
}

// MQTT state changes for the event history, at most one every 5 minutes (a flapping broker would flood it)
bool mqttHistoryUp = false;
unsigned long mqttHistoryAt = 0;
unsigned mqttHistoryChanges = 0;
bool mqttLastUp = false;

static void mqttHistory(unsigned long now)
{
  bool up = client.connected();
  if (up != mqttLastUp)
  {
    mqttLastUp = up;
    mqttHistoryChanges++;
  }
  if (!config.mqttServer[0] || up == mqttHistoryUp || (mqttHistoryAt != 0 && now - mqttHistoryAt < 300000UL))
    return;
  char changes[40] = "";
  if (mqttHistoryChanges > 1)
    snprintf(changes, sizeof(changes), " (%u changes in the last minutes)", mqttHistoryChanges);
  if (up)
    eventAddf("MQTT connected to %s%s", config.mqttServer, changes);
  else
    eventAddf("MQTT lost, state %d%s", client.state(), changes);
  mqttHistoryUp = up;
  mqttHistoryAt = now;
  mqttHistoryChanges = 0;
}

// Forces an immediate MQTT reconnection, eg. after a configuration change.
void mqttReconnectNow()
{
  client.disconnect();
  setupMqttClient();
  mqttLastAttempt = 0;
}

// Scans in the background for a stronger access point of our SSID.
static void roamScanStart()
{
  if (roamScanning || WiFi.scanComplete() != WIFI_SCAN_FAILED)
    return; // a scan is running or its results are pending (web interface)
  roamScanning = WiFi.scanNetworks(true, false, false, 120) == WIFI_SCAN_RUNNING;
}

static void roamScanLoop()
{
  if (!roamScanning)
    return;
  int n = WiFi.scanComplete();
  if (n == WIFI_SCAN_RUNNING)
    return;
  if (n < 0)
  {
    roamScanning = false; // failed
    return;
  }
  NetAp aps[ROAM_SCAN_MAX];
  int count = 0;
  for (int i = 0; i < n && count < ROAM_SCAN_MAX; i++)
  {
    if (WiFi.SSID(i) != config.wifiSsid)
      continue;
    memcpy(aps[count].bssid, WiFi.BSSID(i), 6);
    aps[count].rssi = WiFi.RSSI(i);
    aps[count].channel = WiFi.channel(i);
    count++;
  }
  WiFi.scanDelete();
  roamScanning = false; // until now, the web interface leaves the results alone
  if (WiFi.status() != WL_CONNECTED)
    return;
  int current = WiFi.RSSI();
  int pick = netRoamPick(aps, count, WiFi.BSSID(), current);
  if (pick < 0)
    return;
  const uint8_t *b = aps[pick].bssid;
  netHistory(true, "WiFi weak (%d dBm): moving to access point %02X:%02X:%02X:%02X:%02X:%02X (%d dBm, channel %d)",
             current, b[0], b[1], b[2], b[3], b[4], b[5], aps[pick].rssi, aps[pick].channel);
  netPolicy.noteWifiAttempt(millis()); // a full rescan follows in 15s if this one fails
  WiFi.begin(config.wifiSsid, config.wifiPwd, aps[pick].channel, aps[pick].bssid, true);
}

// Restart cause with the state of the links, eg. "No network for 5 minutes (WiFi AA:BB:.. ch 6 -83 dBm, ...)"
static void netRestart(const char *reason)
{
  char wifi[80] = "";
  if (config.wifiSsid[0])
    snprintf(wifi, sizeof(wifi), "WiFi %s ch %d %d dBm, last disconnect %s", wifiBssid[0] ? wifiBssid : "never joined", wifiChannel, wifiRssi, wifiLastDisconnect ? wifiDisconnectName() : "none");
  const char *eth = "";
#ifdef HAS_ETHERNET
  eth = ethUp ? "Ethernet up" : "Ethernet down";
#endif
  char cause[128];
  if (wifi[0] || eth[0])
    snprintf(cause, sizeof(cause), "%s (%s%s%s)", reason, wifi, wifi[0] && eth[0] ? "; " : "", eth);
  else
    strlcpy(cause, reason, sizeof(cause));
  mqttSerial.println(cause);
  mqttSerial.drain();
  delay(200); // lets the log reach MQTT
  restart_board(cause);
}

void netLoop()
{
  unsigned long now = millis();
  bool wifiUp = WiFi.status() == WL_CONNECTED;
  bool online = wifiUp || ethUp;
  if (wifiUp)
    wifiRssi = WiFi.RSSI();
  if (online != netWasOnline)
  {
    netWasOnline = online;
    if (online)
    {
      if (ethUp)
        netHistory(true, "Online over Ethernet: %s", netIP().toString().c_str());
      else
        netHistory(true, "Online over WiFi: %s, %d dBm", netIP().toString().c_str(), wifiRssi);
      startNetworkServices();
    }
    else
    {
      wakeScreen(); //Show we lost connection
      netHistory(true, "Network connection lost");
    }
  }

  NetInputs in;
  in.now = now;
  in.wifiConfigured = config.wifiSsid[0] != 0;
  in.wifiUp = wifiUp;
  in.rssi = wifiUp ? wifiRssi : 0;
#ifdef HAS_ETHERNET
  in.ethEnabled = true;
#endif
  in.ethUp = ethUp;
  in.apActive = apActive;
  in.apClients = apActive ? WiFi.softAPgetStationNum() : 0;
  in.mqttConfigured = config.mqttServer[0] != 0;
  in.mqttUp = client.connected();
  uint8_t act = netPolicy.step(in);
  if (act & NET_REBOOT)
    netRestart(netPolicy.reason);
  if (netPolicy.reason != nullptr)
    netHistory(true, "%s", netPolicy.reason);
  if (act & NET_STOP_WIFI)
    stopWifi();
  if (act & NET_CONNECT_WIFI)
    connectWifi();
  if (act & NET_START_AP)
    startAccessPoint();
  if (act & NET_STOP_AP)
    stopAccessPoint();
  if (act & NET_ROAM_SCAN)
    roamScanStart();
  roamScanLoop();

  otaConfirmLoop(now, online);
  if (apActive)
    dnsServer.processNextRequest();
  if (online)
    mqttLoop();
  mqttConnectedCache = client.connected();
  mqttStateCache = client.state();
  mqttHistory(now);
}

// Home Assistant / MQTT broker discovery, for the setup wizard.
struct DiscoveredHost
{
  char name[48];
  char ip[16];
  uint16_t port;
  bool homeAssistant;
  bool mqtt; // an MQTT broker answers on port
};

#define MAX_DISCOVERED 8
DiscoveredHost discovered[MAX_DISCOVERED];
int discoveredCount = 0;
volatile bool discoverRequested = false;
volatile bool discoverRunning = false;
bool discoverDone = false;

static bool tcpPortOpen(IPAddress ip, uint16_t port)
{
  WiFiClient c;
  bool open = c.connect(ip, port, 1500);
  c.stop();
  return open;
}

static void addDiscovered(const char *name, IPAddress ip, uint16_t port, bool ha, bool mqtt)
{
  String ipStr = ip.toString();
  for (int i = 0; i < discoveredCount; i++)
  {
    if (strcmp(discovered[i].ip, ipStr.c_str()) == 0)
    {
      discovered[i].homeAssistant |= ha;
      if (mqtt && !discovered[i].mqtt)
      {
        discovered[i].mqtt = true;
        discovered[i].port = port;
      }
      return;
    }
  }
  if (discoveredCount >= MAX_DISCOVERED)
    return;
  DiscoveredHost &h = discovered[discoveredCount++];
  strlcpy(h.name, name, sizeof(h.name));
  strlcpy(h.ip, ipStr.c_str(), sizeof(h.ip));
  h.port = port;
  h.homeAssistant = ha;
  h.mqtt = mqtt;
}

// Arduino core 3.x renamed MDNSResponder::IP() to address()
#if ESP_ARDUINO_VERSION_MAJOR >= 3
#define mdnsIP(i) MDNS.address(i)
#else
#define mdnsIP(i) MDNS.IP(i)
#endif

// Blocks a few seconds: main loop only, on request.
void runDiscovery()
{
  LOOP_STAGE("broker discovery");
  discoverRequested = false;
  discoverRunning = true;
  discoveredCount = 0;
  if (netOnline())
  {
    int n = MDNS.queryService("home-assistant", "tcp");
    for (int i = 0; i < n; i++)
    {
      IPAddress ip = mdnsIP(i);
      // The Mosquitto add-on runs on the Home Assistant host
      bool mqtt = tcpPortOpen(ip, 1883);
      addDiscovered(MDNS.hostname(i).c_str(), ip, mqtt ? 1883 : MDNS.port(i), true, mqtt);
    }
    n = MDNS.queryService("mqtt", "tcp");
    for (int i = 0; i < n; i++)
    {
      addDiscovered(MDNS.hostname(i).c_str(), mdnsIP(i), MDNS.port(i), false, true);
    }
  }
  discoverRunning = false;
  discoverDone = true;
  mqttSerial.printf("Discovery done: %d host(s) found.\n", discoveredCount);
}

#endif
