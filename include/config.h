#ifndef ESPALTHERMA_CONFIG_H
#define ESPALTHERMA_CONFIG_H

// Runtime configuration, persisted in NVS (as JSON) and changed from the web interface.

#include <Arduino.h>
#include "board.h"
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

// Defaults of a freshly installed board.
void configDefaults(AppConfig &c)
{
  memset(&c, 0, sizeof(c));
  strlcpy(c.hostname, "ESPAltherma", sizeof(c.hostname));
  c.layoutCheck = true;
  c.mqttPort = 1883;
  strlcpy(c.mqttClientId, "ESPAltherma-dev", sizeof(c.mqttClientId));
  c.protocol = 'I';
  c.frequency = 30000;
  c.rxPin = BOARD_DEFAULT_RX_PIN;
  c.txPin = BOARD_DEFAULT_TX_PIN;
  c.thermPin = -1;
  c.thermActiveHigh = true;
  c.sg1Pin = -1;
  c.sg2Pin = -1;
  c.sgActiveHigh = true;
  c.safetyPin = -1;
  c.safetyActiveHigh = true;
  strlcpy(c.oneTopicPrefix, "espaltherma/OneATTR/", sizeof(c.oneTopicPrefix));
}

#define CONFIG_NVS_NAMESPACE "espaltherma"

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

// Loads the configuration: the defaults, then what is stored on top.
void configLoad()
{
  configDefaults(config);
  Preferences prefs;
  prefs.begin(CONFIG_NVS_NAMESPACE, false);
  String stored = prefs.getString("cfg", "");
  prefs.end();

  JsonDocument doc;
  bool parsed = stored.length() > 0 && deserializeJson(doc, stored) == DeserializationError::Ok;
  if (parsed)
    configFromJson(config, doc.as<JsonVariantConst>());

  bool newId = config.installId[0] == 0;
  if (newId)
  {
    snprintf(config.installId, sizeof(config.installId), "%08lx%08lx", (unsigned long)esp_random(), (unsigned long)esp_random());
  }
  if (!parsed || newId)
  {
    configSave();
  }
}

// Wipes the persisted configuration: the defaults apply on next boot.
void configReset()
{
  Preferences prefs;
  prefs.begin(CONFIG_NVS_NAMESPACE, false);
  prefs.clear();
  prefs.end();
}

#endif
