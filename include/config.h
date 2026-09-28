#ifndef ESPALTHERMA_CONFIG_H
#define ESPALTHERMA_CONFIG_H

// Runtime configuration.
// Defaults ("seed") come from the #defines of my_setup.h / setup.h so legacy builds behave as before.
// On ESP32 the configuration is persisted in NVS (as JSON) and can be changed at runtime.
// The seed wins again whenever it changes, i.e. when my_setup.h was edited and the firmware re-flashed.

#include <Arduino.h>
#include "board.h"
#ifdef HAS_NVS_CONFIG
#include <Preferences.h>
// Not <ArduinoJson.h>: its "using namespace ArduinoJson" clashes with our Converter class
#include <ArduinoJson.hpp>
using ArduinoJson::DeserializationError;
using ArduinoJson::JsonArray;
using ArduinoJson::JsonArrayConst;
using ArduinoJson::JsonDocument;
using ArduinoJson::JsonObject;
using ArduinoJson::JsonVariantConst;
using ArduinoJson::deserializeJson;
using ArduinoJson::serializeJson;
#endif

#define CONFIG_MAX_LABELS 128
#define CONFIG_MAX_FIXES 4

struct AppConfig
{
  // Network
  char wifiSsid[33];
  char wifiPwd[65];
  bool staticIp;
  uint32_t ip, gateway, subnet, dns1, dns2;
  char hostname[33];
  char lang[6]; // web interface and sensor names: "" follows the browser (sensor names in English), or "fr", "de"...

  // MQTT
  char mqttServer[65];
  uint16_t mqttPort;
  char mqttUser[65];
  char mqttPwd[65];
  bool mqttTls;
  char mqttClientId[33];

  // Heat pump
  char protocol;      // 'I' or 'S'
  uint32_t frequency; // ms between two queries of all registries
  int8_t rxPin;       // Pin connected to the TX pin of X10A
  int8_t txPin;       // Pin connected to the RX pin of X10A

  // Heat pump model of the generic firmware (a definition file name), empty until detected or chosen
  char model[80];
  uint16_t labelCount;                 // 0: the recommended values of the model
  uint32_t labels[CONFIG_MAX_LABELS]; // selected values (catalog keys)
  bool modelConfirmed;                 // chosen by the user, or detected with a high confidence
  char detectedKey[96];                // identification key of the heat pump the model was detected on
  // Registries read with the layout of another definition, because the model's does not match the unit
  // (layoutfix.h). Cleared when the model changes.
  bool layoutCheck; // correct registries automatically
  uint8_t fixCount;
  uint8_t fixReg[CONFIG_MAX_FIXES];
  char fixModel[CONFIG_MAX_FIXES][80];

  // Relays, pin -1 when not used
  int8_t thermPin;
  bool thermActiveHigh;
  int8_t sg1Pin, sg2Pin;
  bool sgActiveHigh;
  int8_t safetyPin;
  bool safetyActiveHigh;

  // MQTT output options
  bool oneValOneTopic;
  char oneTopicPrefix[65];
  bool jsonTable;
  bool disableLogMessages;
  bool debugSerial;

  // Web interface
  char adminPwd[33]; // protects the configuration, updates and OTA when set (user "admin")

  // Opt-in telemetry: sends the heat pump identification to help the auto-detection
  bool telemetry;
  char installId[17]; // random, to deduplicate reports
};

AppConfig config;

// Fills the configuration with the compile time values of my_setup.h / setup.h.
void configSeed(AppConfig &c)
{
  memset(&c, 0, sizeof(c)); // also clears padding, configHash() depends on it

#if defined(WIFI_SSID)
  if (strcmp(WIFI_SSID, "SSID") != 0) // Placeholder of the stock setup.h: leave empty so provisioning kicks in
  {
    strlcpy(c.wifiSsid, WIFI_SSID, sizeof(c.wifiSsid));
#if defined(WIFI_PWD)
    strlcpy(c.wifiPwd, WIFI_PWD, sizeof(c.wifiPwd));
#endif
  }
#endif

#if defined(WIFI_IP) && defined(WIFI_GATEWAY) && defined(WIFI_SUBNET)
  c.staticIp = true;
  c.ip = (uint32_t)IPAddress(WIFI_IP);
  c.gateway = (uint32_t)IPAddress(WIFI_GATEWAY);
  c.subnet = (uint32_t)IPAddress(WIFI_SUBNET);
#if defined(WIFI_PRIMARY_DNS)
  c.dns1 = (uint32_t)IPAddress(WIFI_PRIMARY_DNS);
#endif
#if defined(WIFI_SECONDARY_DNS)
  c.dns2 = (uint32_t)IPAddress(WIFI_SECONDARY_DNS);
#endif
#endif

  strlcpy(c.hostname, "ESPAltherma", sizeof(c.hostname));
  c.lang[0] = 0;
  c.layoutCheck = true;

#if defined(MQTT_SERVER)
  strlcpy(c.mqttServer, MQTT_SERVER, sizeof(c.mqttServer));
#endif
#if defined(MQTT_PORT)
  c.mqttPort = MQTT_PORT;
#else
  c.mqttPort = 1883;
#endif
#if defined(MQTT_USERNAME)
  strlcpy(c.mqttUser, MQTT_USERNAME, sizeof(c.mqttUser));
#endif
#if defined(MQTT_PASSWORD)
  strlcpy(c.mqttPwd, MQTT_PASSWORD, sizeof(c.mqttPwd));
#endif
#if defined(MQTT_ENCRYPTED)
  c.mqttTls = true;
#endif
  strlcpy(c.mqttClientId, "ESPAltherma-dev", sizeof(c.mqttClientId));

#if defined(PROTOCOL)
  c.protocol = PROTOCOL;
#else
  c.protocol = 'I';
#endif
#if defined(FREQUENCY)
  c.frequency = FREQUENCY;
#else
  c.frequency = 30000;
#endif
#if defined(RX_PIN) && defined(TX_PIN)
  c.rxPin = RX_PIN;
  c.txPin = TX_PIN;
#else
  c.rxPin = BOARD_DEFAULT_RX_PIN;
  c.txPin = BOARD_DEFAULT_TX_PIN;
#endif

#if defined(PIN_THERM)
  c.thermPin = PIN_THERM;
  c.thermActiveHigh = PIN_THERM_ACTIVE_STATE == HIGH;
#else
  c.thermPin = -1;
  c.thermActiveHigh = true;
#endif

#if defined(PIN_SG1) && defined(PIN_SG2)
  c.sg1Pin = PIN_SG1;
  c.sg2Pin = PIN_SG2;
#else
  c.sg1Pin = -1;
  c.sg2Pin = -1;
#endif
#if defined(SG_RELAY_ACTIVE_STATE)
  c.sgActiveHigh = SG_RELAY_ACTIVE_STATE == HIGH;
#else
  c.sgActiveHigh = true;
#endif

#if defined(SAFETY_RELAY_PIN)
  c.safetyPin = SAFETY_RELAY_PIN;
  c.safetyActiveHigh = SAFETY_RELAY_ACTIVE_STATE == HIGH;
#else
  c.safetyPin = -1;
  c.safetyActiveHigh = true;
#endif

#if defined(ONEVAL_ONETOPIC)
  c.oneValOneTopic = true;
#endif
#if defined(MQTT_OneTopic)
  strlcpy(c.oneTopicPrefix, MQTT_OneTopic, sizeof(c.oneTopicPrefix));
#else
  strlcpy(c.oneTopicPrefix, "espaltherma/OneATTR/", sizeof(c.oneTopicPrefix));
#endif
#if defined(JSONTABLE)
  c.jsonTable = true;
#endif
#if defined(DISABLE_LOG_MESSAGES)
  c.disableLogMessages = true;
#endif
#if defined(DEBUG_SERIAL)
  c.debugSerial = true;
#endif
}

#ifdef HAS_NVS_CONFIG

#define CONFIG_NVS_NAMESPACE "espaltherma"

static void configHashBytes(uint32_t &h, const void *data, size_t len)
{
  const uint8_t *p = static_cast<const uint8_t *>(data);
  for (size_t i = 0; i < len; i++)
  {
    h ^= p[i];
    h *= 16777619u;
  }
}

// FNV-1a of the values my_setup.h sets, to find out if it changed since the last boot.
// Field by field, not the whole structure: a firmware that adds a setting must not look like an edited my_setup.h
// (that re-seeded, and wiped what was set in the web interface).
uint32_t configHash(const AppConfig &c)
{
  uint32_t h = 2166136261u;
  auto str = [&](const char *v) { configHashBytes(h, v, strlen(v) + 1); };
  auto num = [&](int32_t v) { configHashBytes(h, &v, sizeof(v)); };
  str(c.wifiSsid); str(c.wifiPwd);
  num(c.staticIp); num(c.ip); num(c.gateway); num(c.subnet); num(c.dns1); num(c.dns2);
  str(c.hostname);
  str(c.mqttServer); num(c.mqttPort); str(c.mqttUser); str(c.mqttPwd); num(c.mqttTls); str(c.mqttClientId);
  num(c.protocol); num(c.frequency); num(c.rxPin); num(c.txPin);
  num(c.thermPin); num(c.thermActiveHigh); num(c.sg1Pin); num(c.sg2Pin); num(c.sgActiveHigh);
  num(c.safetyPin); num(c.safetyActiveHigh);
  num(c.oneValOneTopic); str(c.oneTopicPrefix); num(c.jsonTable); num(c.disableLogMessages); num(c.debugSerial);
  return h;
}

static String ipToString(uint32_t ip)
{
  return ip == 0 ? String("") : IPAddress(ip).toString();
}

static uint32_t ipFromString(const char *s)
{
  IPAddress ip;
  if (s == nullptr || !ip.fromString(s))
    return 0;
  return (uint32_t)ip;
}

// Serializes the configuration. Secrets (passwords) are only included when asked, eg. to persist them.
void configToJson(const AppConfig &c, JsonDocument &doc, bool includeSecrets)
{
  JsonObject wifi = doc["wifi"].to<JsonObject>();
  wifi["ssid"] = c.wifiSsid;
  if (includeSecrets)
    wifi["pwd"] = c.wifiPwd;
  wifi["static"] = c.staticIp;
  wifi["ip"] = ipToString(c.ip);
  wifi["gateway"] = ipToString(c.gateway);
  wifi["subnet"] = ipToString(c.subnet);
  wifi["dns1"] = ipToString(c.dns1);
  wifi["dns2"] = ipToString(c.dns2);
  doc["hostname"] = c.hostname;
  doc["lang"] = c.lang;

  JsonObject mqtt = doc["mqtt"].to<JsonObject>();
  mqtt["server"] = c.mqttServer;
  mqtt["port"] = c.mqttPort;
  mqtt["user"] = c.mqttUser;
  if (includeSecrets)
    mqtt["pwd"] = c.mqttPwd;
  mqtt["tls"] = c.mqttTls;
  mqtt["client_id"] = c.mqttClientId;

  JsonObject hp = doc["hp"].to<JsonObject>();
  hp["protocol"] = String(c.protocol);
  hp["frequency"] = c.frequency;
  hp["rx"] = c.rxPin;
  hp["tx"] = c.txPin;
  hp["model"] = c.model;
  hp["confirmed"] = c.modelConfirmed;
  hp["detected_key"] = c.detectedKey;
  hp["layout_check"] = c.layoutCheck;
  JsonArray fixes = hp["layout_fixes"].to<JsonArray>();
  for (uint8_t i = 0; i < c.fixCount; i++)
  {
    JsonObject fix = fixes.add<JsonObject>();
    fix["reg"] = c.fixReg[i];
    fix["model"] = c.fixModel[i];
  }
  JsonArray labels = hp["labels"].to<JsonArray>();
  for (uint16_t i = 0; i < c.labelCount; i++)
  {
    labels.add(c.labels[i]);
  }

  JsonObject relays = doc["relays"].to<JsonObject>();
  relays["therm_pin"] = c.thermPin;
  relays["therm_high"] = c.thermActiveHigh;
  relays["sg1_pin"] = c.sg1Pin;
  relays["sg2_pin"] = c.sg2Pin;
  relays["sg_high"] = c.sgActiveHigh;
  relays["safety_pin"] = c.safetyPin;
  relays["safety_high"] = c.safetyActiveHigh;

  JsonObject out = doc["output"].to<JsonObject>();
  out["one_topic"] = c.oneValOneTopic;
  out["one_topic_prefix"] = c.oneTopicPrefix;
  out["json_table"] = c.jsonTable;
  out["no_log"] = c.disableLogMessages;
  out["debug_serial"] = c.debugSerial;

  JsonObject admin = doc["admin"].to<JsonObject>();
  admin["has_pwd"] = c.adminPwd[0] != 0;
  if (includeSecrets)
    admin["pwd"] = c.adminPwd;

  JsonObject telemetry = doc["telemetry"].to<JsonObject>();
  telemetry["enabled"] = c.telemetry;
  telemetry["install_id"] = c.installId;

  if (!includeSecrets)
  {
    // Tells the web interface that a password is set, without sending it
    wifi["has_pwd"] = c.wifiPwd[0] != 0;
    mqtt["has_pwd"] = c.mqttPwd[0] != 0;
  }
}

static void readStr(JsonVariantConst v, char *dst, size_t size)
{
  if (v.is<const char *>())
    strlcpy(dst, v.as<const char *>(), size);
}

template <typename T>
static void readNum(JsonVariantConst v, T &dst)
{
  if (v.is<T>())
    dst = v.as<T>();
}

static void readIp(JsonVariantConst v, uint32_t &dst)
{
  if (v.is<const char *>())
    dst = ipFromString(v.as<const char *>());
}

// Applies the keys present in src on top of c. Missing keys keep their current value.
void configFromJson(AppConfig &c, JsonVariantConst src)
{
  JsonVariantConst wifi = src["wifi"];
  readStr(wifi["ssid"], c.wifiSsid, sizeof(c.wifiSsid));
  readStr(wifi["pwd"], c.wifiPwd, sizeof(c.wifiPwd));
  readNum(wifi["static"], c.staticIp);
  readIp(wifi["ip"], c.ip);
  readIp(wifi["gateway"], c.gateway);
  readIp(wifi["subnet"], c.subnet);
  readIp(wifi["dns1"], c.dns1);
  readIp(wifi["dns2"], c.dns2);
  readStr(src["hostname"], c.hostname, sizeof(c.hostname));
  readStr(src["lang"], c.lang, sizeof(c.lang));

  JsonVariantConst mqtt = src["mqtt"];
  readStr(mqtt["server"], c.mqttServer, sizeof(c.mqttServer));
  readNum(mqtt["port"], c.mqttPort);
  readStr(mqtt["user"], c.mqttUser, sizeof(c.mqttUser));
  readStr(mqtt["pwd"], c.mqttPwd, sizeof(c.mqttPwd));
  readNum(mqtt["tls"], c.mqttTls);
  readStr(mqtt["client_id"], c.mqttClientId, sizeof(c.mqttClientId));

  JsonVariantConst hp = src["hp"];
  if (hp["protocol"].is<const char *>())
  {
    char p = hp["protocol"].as<const char *>()[0];
    if (p == 'I' || p == 'S')
      c.protocol = p;
  }
  readNum(hp["frequency"], c.frequency);
  readNum(hp["rx"], c.rxPin);
  readNum(hp["tx"], c.txPin);
  readStr(hp["model"], c.model, sizeof(c.model));
  readNum(hp["confirmed"], c.modelConfirmed);
  readStr(hp["detected_key"], c.detectedKey, sizeof(c.detectedKey));
  readNum(hp["layout_check"], c.layoutCheck);
  if (hp["layout_fixes"].is<JsonArrayConst>())
  {
    c.fixCount = 0;
    for (JsonVariantConst v : hp["layout_fixes"].as<JsonArrayConst>())
    {
      if (c.fixCount < CONFIG_MAX_FIXES && v["reg"].is<uint8_t>() && v["model"].is<const char *>())
      {
        c.fixReg[c.fixCount] = v["reg"].as<uint8_t>();
        strlcpy(c.fixModel[c.fixCount], v["model"].as<const char *>(), sizeof(c.fixModel[0]));
        c.fixCount++;
      }
    }
  }
  if (hp["labels"].is<JsonArrayConst>())
  {
    c.labelCount = 0;
    for (JsonVariantConst v : hp["labels"].as<JsonArrayConst>())
    {
      if (c.labelCount < CONFIG_MAX_LABELS && v.is<uint32_t>())
        c.labels[c.labelCount++] = v.as<uint32_t>();
    }
  }

  JsonVariantConst relays = src["relays"];
  readNum(relays["therm_pin"], c.thermPin);
  readNum(relays["therm_high"], c.thermActiveHigh);
  readNum(relays["sg1_pin"], c.sg1Pin);
  readNum(relays["sg2_pin"], c.sg2Pin);
  readNum(relays["sg_high"], c.sgActiveHigh);
  readNum(relays["safety_pin"], c.safetyPin);
  readNum(relays["safety_high"], c.safetyActiveHigh);

  JsonVariantConst out = src["output"];
  readNum(out["one_topic"], c.oneValOneTopic);
  readStr(out["one_topic_prefix"], c.oneTopicPrefix, sizeof(c.oneTopicPrefix));
  readNum(out["json_table"], c.jsonTable);
  readNum(out["no_log"], c.disableLogMessages);
  readNum(out["debug_serial"], c.debugSerial);

  readStr(src["admin"]["pwd"], c.adminPwd, sizeof(c.adminPwd));
  readNum(src["telemetry"]["enabled"], c.telemetry);
  readStr(src["telemetry"]["install_id"], c.installId, sizeof(c.installId));
}

void configSave()
{
  JsonDocument doc;
  configToJson(config, doc, true);
  String s;
  serializeJson(doc, s);
  Preferences prefs;
  prefs.begin(CONFIG_NVS_NAMESPACE, false);
  prefs.putString("cfg", s);
  prefs.end();
}

// Loads the configuration: the seed if it changed since last boot, the persisted one otherwise.
void configLoad()
{
  configSeed(config);
  uint32_t seedHash = configHash(config);

  Preferences prefs;
  prefs.begin(CONFIG_NVS_NAMESPACE, false);
#ifdef ESPALTHERMA_GENERIC
  // No compile time settings: the stored configuration always wins
  bool seedChanged = false;
#else
  bool seedChanged = prefs.getUInt("seed", 0) != seedHash;
#endif
  String stored = prefs.getString("cfg", "");
  if (seedChanged)
  {
    prefs.putUInt("seed", seedHash);
  }
  prefs.end();

  JsonDocument doc;
  bool parsed = stored.length() > 0 && deserializeJson(doc, stored) == DeserializationError::Ok;
  if (parsed)
  {
    if (seedChanged)
      readStr(doc["telemetry"]["install_id"], config.installId, sizeof(config.installId)); // keep the identity
    else
      configFromJson(config, doc.as<JsonVariantConst>());
  }

  bool newId = config.installId[0] == 0;
  if (newId)
  {
    snprintf(config.installId, sizeof(config.installId), "%08lx%08lx", (unsigned long)esp_random(), (unsigned long)esp_random());
  }
  if (seedChanged || !parsed || newId)
  {
    configSave();
  }
}

// Wipes the persisted configuration, the seed is used on next boot.
void configReset()
{
  Preferences prefs;
  prefs.begin(CONFIG_NVS_NAMESPACE, false);
  prefs.clear();
  prefs.end();
}

#else

void configLoad()
{
  configSeed(config);
}

void configSave() {}

#endif // HAS_NVS_CONFIG

#endif
