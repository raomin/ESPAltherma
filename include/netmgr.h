#ifndef ESPALTHERMA_NETMGR_H
#define ESPALTHERMA_NETMGR_H

// Network manager (ESP32): never blocks the main loop.
//  - WiFi station, retried every 15s. Without a connection for 2 minutes (or without any WiFi configured),
//    an access point "ESPAltherma-XXXX" is started with a captive portal serving the web interface.
//  - mDNS (espaltherma.local) and ArduinoOTA once connected.
//  - MQTT: one connection attempt every 5s, when configured.
//  - Discovery of Home Assistant / MQTT brokers on the network (mDNS), for the setup wizard.

#include <WiFi.h>
#include <DNSServer.h>
#include <ESPmDNS.h>

#define AP_PASSWORD "espaltherma" // WPA2 key of the setup access point (documented, shown on screen/serial)
#define WIFI_RETRY_MS 15000UL
#define AP_FALLBACK_MS 120000UL
#define AP_IDLE_STOP_MS 300000UL
#define WIFI_REBOOT_MS (30UL * 60 * 1000)
#define MQTT_RETRY_MS 5000UL
#define MQTT_REBOOT_MS (15UL * 60 * 1000)

// Defined in main.cpp
void checkWifiRoaming();
void wakeScreen();

DNSServer dnsServer;
bool apActive = false;
char apSsid[33] = "";
bool staConnected = false;
unsigned long staLostSince = 0;
unsigned long staLastAttempt = 0;
unsigned long staConnectedSince = 0;
bool networkServicesStarted = false;
unsigned long mqttLastAttempt = 0;
unsigned long mqttFailingSince = 0;
// MQTT status for the web interface: PubSubClient must only be used from the main loop
volatile bool mqttConnectedCache = false;
volatile int mqttStateCache = MQTT_DISCONNECTED;

void startAccessPoint()
{
  if (apActive)
    return;
  uint8_t mac[6];
  WiFi.macAddress(mac);
  snprintf(apSsid, sizeof(apSsid), "ESPAltherma-%02X%02X", mac[4], mac[5]);
  WiFi.mode(config.wifiSsid[0] ? WIFI_AP_STA : WIFI_AP);
  WiFi.softAP(apSsid, AP_PASSWORD);
  dnsServer.start(53, "*", WiFi.softAPIP());
  apActive = true;
  wakeScreen();
  mqttSerial.printf("Setup access point: %s (password: %s), open http://%s\n", apSsid, AP_PASSWORD, WiFi.softAPIP().toString().c_str());
}

void stopAccessPoint()
{
  if (!apActive)
    return;
  dnsServer.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  apActive = false;
  mqttSerial.println("Setup access point stopped.");
}

// (Re)connects the station with the configured credentials.
void connectWifi()
{
  staLastAttempt = millis();
  if (!config.wifiSsid[0])
    return;
  mqttSerial.printf("Connecting to %s\n", config.wifiSsid);
  if (apActive)
    WiFi.mode(WIFI_AP_STA); // credentials entered on the setup access point: the station must be enabled
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

void netBegin()
{
  WiFi.persistent(false);
  WiFi.setHostname(config.hostname);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);
  WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
  staLostSince = millis();
  if (config.wifiSsid[0])
    connectWifi();
  else
    startAccessPoint();
}

// OTA and mDNS, started with the first station connection.
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
}

void mqttLoop()
{
  if (!config.mqttServer[0])
    return;
  if (client.connected())
  {
    mqttFailingSince = 0;
    return;
  }
  unsigned long now = millis();
  if (mqttLastAttempt != 0 && now - mqttLastAttempt < MQTT_RETRY_MS)
    return;
  mqttLastAttempt = now;
  if (mqttConnectOnce())
  {
    mqttFailingSince = 0;
    return;
  }
  if (mqttFailingSince == 0)
    mqttFailingSince = now;
  else if (now - mqttFailingSince > MQTT_REBOOT_MS)
  {
    mqttSerial.println("MQTT unreachable for 15 minutes, rebooting now.");
    restart_board();
  }
}

// Forces an immediate MQTT reconnection, eg. after a configuration change.
void mqttReconnectNow()
{
  client.disconnect();
  setupMqttClient();
  mqttLastAttempt = 0;
  mqttFailingSince = 0;
}

void netLoop()
{
  unsigned long now = millis();
  bool connected = WiFi.status() == WL_CONNECTED;
  if (connected != staConnected)
  {
    staConnected = connected;
    if (connected)
    {
      staConnectedSince = now;
      mqttSerial.printf("Connected. IP Address: %s\n", WiFi.localIP().toString().c_str());
      startNetworkServices();
    }
    else
    {
      staLostSince = now;
      wakeScreen(); //Show we lost connection
      mqttSerial.println("WiFi connection lost.");
    }
  }

  if (!connected && config.wifiSsid[0])
  {
    if (now - staLastAttempt >= WIFI_RETRY_MS)
    { //Auto-reconnect is not making it: force a full scan so we reattach to the strongest AP
      connectWifi();
    }
    if (!apActive && now - staLostSince >= AP_FALLBACK_MS)
    {
      startAccessPoint();
    }
    if (now - staLostSince >= WIFI_REBOOT_MS && (!apActive || WiFi.softAPgetStationNum() == 0))
    { //Still no WiFi and nobody on the setup access point: reboot in case the WiFi stack is wedged
      mqttSerial.println("No WiFi for 30 minutes, rebooting now.");
      restart_board();
    }
  }

  if (apActive)
  {
    dnsServer.processNextRequest();
    if (connected && now - staConnectedSince > AP_IDLE_STOP_MS && WiFi.softAPgetStationNum() == 0)
    {
      stopAccessPoint();
    }
  }

  if (connected)
  {
    if (!apActive)
      checkWifiRoaming(); //Move to a stronger AP of the same SSID if signal got weak
    mqttLoop();
  }
  mqttConnectedCache = client.connected();
  mqttStateCache = client.state();
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

// Blocks a few seconds: main loop only, on request.
void runDiscovery()
{
  discoverRequested = false;
  discoverRunning = true;
  discoveredCount = 0;
  if (WiFi.status() == WL_CONNECTED)
  {
    int n = MDNS.queryService("home-assistant", "tcp");
    for (int i = 0; i < n; i++)
    {
      IPAddress ip = MDNS.IP(i);
      // The Mosquitto add-on runs on the Home Assistant host
      bool mqtt = tcpPortOpen(ip, 1883);
      addDiscovered(MDNS.hostname(i).c_str(), ip, mqtt ? 1883 : MDNS.port(i), true, mqtt);
    }
    n = MDNS.queryService("mqtt", "tcp");
    for (int i = 0; i < n; i++)
    {
      addDiscovered(MDNS.hostname(i).c_str(), MDNS.IP(i), MDNS.port(i), false, true);
    }
  }
  discoverRunning = false;
  discoverDone = true;
  mqttSerial.printf("Discovery done: %d host(s) found.\n", discoveredCount);
}

#endif
