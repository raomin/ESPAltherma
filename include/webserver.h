#ifndef ESPALTHERMA_WEBSERVER_H
#define ESPALTHERMA_WEBSERVER_H

// Web interface (ESP32): the page (web/index.html, embedded gzipped), a JSON API and live events.
// Request handlers run in the async TCP task: they never block and never change the running state
// directly. Changes are queued and applied by webLoop() in the main loop.

#include <ESPAsyncWebServer.h>
#include <memory>
#include <Update.h>
#include "webui_data.h"
#include <HTTPClient.h>

#ifndef TELEMETRY_URL
#define TELEMETRY_URL "" // Collection endpoint of the opt-in telemetry; nothing is sent while empty
#endif
#define TELEMETRY_AVAILABLE (strlen(TELEMETRY_URL) > 0)

#define MAX_BODY_SIZE 8192

// Defined in main.cpp
extern Survey survey;
extern char surveyJson[];
extern volatile bool labelsDirty;
extern volatile bool forceDetection;
extern volatile bool surveyRequested;
extern volatile bool surveyInProgress;
extern unsigned long lastPollMs;
extern DetectResult detection;
void requestSurvey();
void valuesLock();
void valuesUnlock();

// Last reply of every registry polled: the Values page shows the current value of any entry, selected or not
struct ReplyCache
{
  uint8_t count = 0;
  uint8_t reg[24];
  uint8_t len[24];
  uint8_t data[24][SURVEY_MAX_PAYLOAD];

  void update(uint8_t r, const uint8_t *payload, uint8_t n)
  {
    uint8_t i = 0;
    while (i < count && reg[i] != r)
      i++;
    if (i == count)
    {
      if (count >= 24)
        return;
      count++;
    }
    reg[i] = r;
    len[i] = n > SURVEY_MAX_PAYLOAD ? SURVEY_MAX_PAYLOAD : n;
    memcpy(data[i], payload, len[i]);
  }

  // Payload of a registry: the last reply, or the survey's when the registry is not polled. nullptr when unknown.
  const uint8_t *find(uint8_t r, uint8_t &n, const Survey &survey) const
  {
    for (uint8_t i = 0; i < count; i++)
    {
      if (reg[i] == r)
      {
        n = len[i];
        return data[i];
      }
    }
    const SurveyReg *sr = surveyInProgress ? nullptr : survey.find(r); // being rewritten by the heat pump task
    if (sr != nullptr && sr->answered)
    {
      n = sr->len;
      return sr->payload;
    }
    return nullptr;
  }
} replyCache;

AsyncWebServer webServer(80);
AsyncEventSource webEvents("/events");
LogRing logHistory; // recent log, for the diagnostics page

AppConfig *volatile pendingConfig = nullptr; // posted configuration, applied by webLoop
volatile bool rebootRequested = false;
unsigned long rebootAt = 0;
volatile bool factoryResetRequested = false;
const char *volatile rebootCause = "Restart from the web interface"; // restart cause saved for the next boot
volatile bool mqttTestRequested = false;
AppConfig mqttTestConfig;
char mqttTestResult[16] = ""; // "running", "ok" or "error"
const char *mqttTestReason = "";  // English, translated by the web interface
int mqttTestState = 0;             // PubSubClient state
volatile bool telemetryPending = false;
bool updateFailed = false;

static bool authorized(AsyncWebServerRequest *r)
{
  if (!config.adminPwd[0])
    return true;
  if (r->authenticate("admin", config.adminPwd))
    return true;
  r->requestAuthentication();
  return false;
}

static void sendJson(AsyncWebServerRequest *r, JsonDocument &doc, int code = 200)
{
  String s;
  serializeJson(doc, s);
  r->send(code, "application/json", s);
}

static void sendOk(AsyncWebServerRequest *r)
{
  r->send(200, "application/json", "{\"ok\":true}");
}

// Collects a request body into _tempObject (freed with the request).
static void collectBody(AsyncWebServerRequest *r, uint8_t *data, size_t len, size_t index, size_t total)
{
  if (total > MAX_BODY_SIZE)
    return;
  if (index == 0)
    r->_tempObject = calloc(total + 1, 1);
  if (r->_tempObject != nullptr)
    memcpy((uint8_t *)r->_tempObject + index, data, len);
}

static bool parseBody(AsyncWebServerRequest *r, JsonDocument &doc)
{
  if (r->_tempObject == nullptr || deserializeJson(doc, (const char *)r->_tempObject) != DeserializationError::Ok)
  {
    r->send(400, "application/json", "{\"ok\":false,\"error\":\"invalid JSON\"}");
    return false;
  }
  return true;
}

// Called from mqttSerial.drain() (main loop) for each log chunk.
void webLogChunk(const char *chunk, size_t len)
{
  logHistory.push((const uint8_t *)chunk, len);
  if (webEvents.count() > 0)
    webEvents.send(chunk, "log");
}

void webSendValues()
{
  if (webEvents.count() == 0)
    return;
  // One document: serializeJson() overwrites a String, it does not append to it
  JsonDocument doc;
  JsonArray values = doc.to<JsonArray>();
  valuesLock();
  for (size_t i = 0; i < converter.activeCount; i++)
  {
    const LabelDef &l = converter.activeLabels[i];
    JsonObject item = values.add<JsonObject>();
    item["l"] = l.label;
    item["v"] = l.asString;
    item["r"] = l.registryID;
  }
  valuesUnlock();
  String s;
  serializeJson(doc, s);
  webEvents.send(s.c_str(), "values");
}

static void statusJson(JsonDocument &doc)
{
  doc["fw"] = ESPALTHERMA_VERSION;
  doc["board"] = BOARD_NAME;
  doc["uptime"] = millis() / 1000;
  doc["heap"] = ESP.getFreeHeap();
  doc["heap_min"] = ESP.getMinFreeHeap();
  doc["hostname"] = config.hostname;

  JsonObject net = doc["net"].to<JsonObject>();
  net["link"] = netLinkName(); // "wifi", "ethernet" or "none"
  net["ip"] = netIP().toString();

  JsonObject wifi = doc["wifi"].to<JsonObject>();
  wifi["ssid"] = config.wifiSsid;
  wifi["connected"] = WiFi.status() == WL_CONNECTED;
  wifi["ip"] = WiFi.localIP().toString();
  wifi["rssi"] = WiFi.RSSI();
  wifi["bssid"] = wifiBssid; // access point last joined
  wifi["channel"] = (int)wifiChannel;
  wifi["last_disconnect"] = wifiDisconnectName();
  wifi["ap"] = apActive;
  wifi["ap_ssid"] = apSsid;
  wifi["ap_ip"] = WiFi.softAPIP().toString();
#ifdef HAS_ETHERNET
  JsonObject eth = doc["eth"].to<JsonObject>();
  eth["up"] = (bool)ethUp;
  eth["ip"] = ETH.localIP().toString();
  eth["mac"] = ETH.macAddress();
  eth["speed"] = ETH.linkSpeed();
  eth["full_duplex"] = ETH.fullDuplex();
#endif

  JsonObject mqtt = doc["mqtt"].to<JsonObject>();
  mqtt["configured"] = config.mqttServer[0] != 0;
  mqtt["connected"] = (bool)mqttConnectedCache;
  mqtt["server"] = config.mqttServer;
  mqtt["state"] = (int)mqttStateCache;

  JsonObject hp = doc["hp"].to<JsonObject>();
  hp["protocol"] = String(config.protocol);
  hp["model"] = config.model;
  hp["confirmed"] = config.modelConfirmed;
  hp["values"] = converter.activeCount;
  hp["last_poll"] = lastPollMs ? (long)((millis() - lastPollMs) / 1000) : -1;
  hp["survey_running"] = surveyRequested || surveyInProgress;
  hp["survey_done"] = surveyJson[0] != 0;
  JsonArray fixes = hp["layout_fixes"].to<JsonArray>();
  for (uint8_t f = 0; f < config.fixCount; f++)
  {
    JsonObject fix = fixes.add<JsonObject>();
    fix["reg"] = config.fixReg[f];
    fix["model"] = config.fixModel[f];
  }
  if (detection.model >= 0)
  {
    JsonObject d = doc["detect"].to<JsonObject>();
    d["model"] = CATALOG_MODELS[detection.model].name;
    d["family"] = String(CATALOG_MODELS[detection.model].family);
    d["confidence"] = detectConfidenceName(detection.confidence);
  }
  doc["telemetry_available"] = TELEMETRY_AVAILABLE;
  doc["reset_reason"] = resetReasonName();
  doc["restart_cause"] = restartCause; // saved by the firmware before restarting itself, "" otherwise
  JsonObject relays = doc["relays"].to<JsonObject>();
  if (config.thermPin >= 0)
    relays["thermostat"] = thermostatOn;
  if (config.safetyPin >= 0)
    relays["safety"] = safetyActive;
  if (config.sg1Pin >= 0)
    relays["smart_grid"] = sgMode;
#ifdef HAS_POWER_MONITOR
  JsonObject pw = doc["power"].to<JsonObject>();
  if (power.vbus >= 0)
  {
    pw["vbus"] = power.vbus / 1000.0;
    pw["vbus_min"] = power.vbusMin / 1000.0;
    pw["source"] = power.source;
    if (power.supplyCurrent >= 0)
      pw["current"] = power.supplyCurrent;
  }
  if (power.battery >= 0)
  {
    pw["battery"] = power.battery / 1000.0;
    pw["battery_level"] = power.batteryLevel;
    pw["battery_current"] = power.batteryCurrent;
    pw["charging"] = power.charging;
  }
#endif
}

// The exact payload of the opt-in telemetry: the survey report, the chosen model, and the random install id.
String telemetryPayload()
{
  String s = "{\"install_id\":\"";
  s += config.installId;
  s += "\",\"model\":\"";
  s += config.model;
  s += "\",\"confirmed\":";
  s += config.modelConfirmed ? "true" : "false";
  s += ",\"layout_fixes\":[";
  for (uint8_t f = 0; f < config.fixCount; f++)
  {
    s += f ? "," : "";
    s += "{\"reg\":" + String(config.fixReg[f]) + ",\"model\":\"" + String(config.fixModel[f]) + "\"}";
  }
  s += "]";
  s += ",\"report\":";
  valuesLock();
  s += surveyJson[0] ? surveyJson : "null";
  valuesUnlock();
  s += "}";
  return s;
}

// The catalog of a model, sent item by item: the whole list (25KB with the translated names) may not fit in one
// piece of a fragmented heap, which crashed the device when it was built in memory first.
struct CatalogStream
{
  int model = -1;
  int language = -1;
  bool current = false;
  CatalogFix fixes[CONFIG_MAX_FIXES];
  size_t fixCount = 0;
  int next = 0; // next entry to look at
  bool started = false;
  bool ended = false;
  bool firstItem = true;
  String pending; // bytes of the current piece not sent yet
  size_t sent = 0;
  ::Converter decoder; // one-off decodes of the current values

  // Prepares the next piece: "[", an item, or "]". False when everything was sent.
  bool produce()
  {
    pending = "";
    sent = 0;
    if (!started)
    {
      started = true;
      pending = "[";
      return true;
    }
    for (; next < CATALOG_ENTRY_COUNT; next++)
    {
      const CatalogEntry &e = CATALOG_ENTRIES[next];
      if (!catalogInModel(e, catalogModelFor(e.reg, model, fixes, fixCount)) || (e.flags & CATALOG_FLAG_ALWAYS))
        continue;
      next++;
      uint32_t key = catalogKey(e);
      bool recommended = e.flags & CATALOG_FLAG_RECOMMENDED;
      bool selected = recommended;
      if (current && config.labelCount > 0)
      {
        selected = false;
        for (uint16_t k = 0; k < config.labelCount && !selected; k++)
          selected = config.labels[k] == key;
      }
      JsonDocument item;
      item["k"] = key;
      item["r"] = e.reg;
      item["o"] = e.offset;
      item["c"] = e.conv;
      item["l"] = catalogLabel(e);
      const char *translated = catalogName(e, language);
      if (translated != catalogLabel(e))
        item["n"] = translated; // name in the requested language
      item["rec"] = recommended;
      item["sel"] = selected;
      uint8_t len = 0;
      valuesLock();
      const uint8_t *payload = replyCache.find(e.reg, len, survey);
      if (payload != nullptr && e.offset + e.size <= len)
      {
        LabelDef value(e.reg, e.offset, e.conv, e.size, e.type, catalogLabel(e));
        memset(value.asString, 0, sizeof(value.asString));
        decoder.convert(&value, const_cast<uint8_t *>(payload + e.offset)); // read only, bounds checked above
        item["v"] = String(value.asString);
      }
      valuesUnlock();
      String json;
      serializeJson(item, json); // overwrites json: the comma goes in front afterwards
      pending = firstItem ? json : "," + json;
      firstItem = false;
      return true;
    }
    if (ended)
      return false;
    ended = true;
    pending = "]";
    return true;
  }

  size_t fill(uint8_t *buffer, size_t maxLen)
  {
    size_t n = 0;
    while (n < maxLen)
    {
      if (sent >= pending.length() && !produce())
        break;
      size_t chunk = pending.length() - sent;
      if (chunk > maxLen - n)
        chunk = maxLen - n;
      memcpy(buffer + n, pending.c_str() + sent, chunk);
      n += chunk;
      sent += chunk;
    }
    return n;
  }
};

static void handleCatalog(AsyncWebServerRequest *r)
{
  String name = r->hasParam("model") ? r->getParam("model")->value() : String(config.model);
  int model = catalogFindModel(name.c_str());
  if (model < 0)
  {
    r->send(404, "application/json", "{\"ok\":false,\"error\":\"unknown model\"}");
    return;
  }
  std::shared_ptr<CatalogStream> stream = std::make_shared<CatalogStream>();
  stream->model = model;
  stream->current = strcmp(name.c_str(), config.model) == 0;
  String lang = r->hasParam("lang") ? r->getParam("lang")->value() : String(config.lang);
  stream->language = catalogLanguage(lang.c_str());
  for (uint8_t f = 0; stream->current && f < config.fixCount; f++)
  {
    int fixModel = catalogFindModel(config.fixModel[f]);
    if (fixModel >= 0)
      stream->fixes[stream->fixCount++] = {config.fixReg[f], fixModel};
  }
  stream->decoder.quiet = true;
  stream->decoder.RType = converter.RType;
  r->sendChunked("application/json", [stream](uint8_t *buffer, size_t maxLen, size_t) -> size_t {
    return stream->fill(buffer, maxLen);
  });
}

void webBegin()
{
  mqttSerial.onChunk = webLogChunk;

  webServer.on("/", HTTP_GET, [](AsyncWebServerRequest *r) {
    AsyncWebServerResponse *resp = r->beginResponse(200, "text/html", WEBUI_INDEX_GZ, WEBUI_INDEX_GZ_LEN);
    resp->addHeader("Content-Encoding", "gzip");
    r->send(resp);
  });

  webServer.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *r) {
    JsonDocument doc;
    statusJson(doc);
    sendJson(r, doc);
  });

  webServer.on("/api/config", HTTP_GET, [](AsyncWebServerRequest *r) {
    if (!authorized(r))
      return;
    JsonDocument doc;
    configToJson(config, doc, false);
    sendJson(r, doc);
  });

  webServer.on("/api/config", HTTP_POST, [](AsyncWebServerRequest *r) {
    if (!authorized(r))
      return;
    JsonDocument doc;
    if (!parseBody(r, doc))
      return;
    if (pendingConfig != nullptr)
    {
      r->send(409, "application/json", "{\"ok\":false,\"error\":\"busy, retry\"}");
      return;
    }
    AppConfig *next = new AppConfig(config);
    configFromJson(*next, doc.as<JsonVariantConst>());
    if (strcmp(next->model, config.model) != 0 && !doc["hp"]["layout_fixes"].is<JsonArrayConst>())
      next->fixCount = 0; // the corrections belong to the previous model
    bool reboot = next->rxPin != config.rxPin || next->txPin != config.txPin || next->thermPin != config.thermPin ||
                  next->sg1Pin != config.sg1Pin || next->sg2Pin != config.sg2Pin || next->safetyPin != config.safetyPin ||
                  strcmp(next->hostname, config.hostname) != 0 || strcmp(next->adminPwd, config.adminPwd) != 0;
    pendingConfig = next;
    JsonDocument res;
    res["ok"] = true;
    res["reboot"] = reboot;
    sendJson(r, res);
  }, nullptr, collectBody);

  webServer.on("/api/detect", HTTP_GET, [](AsyncWebServerRequest *r) {
    valuesLock();
    String report = surveyJson[0] ? surveyJson : "{}";
    valuesUnlock();
    r->send(200, "application/json", report);
  });

  webServer.on("/api/detect", HTTP_POST, [](AsyncWebServerRequest *r) {
    if (!authorized(r))
      return;
    forceDetection = true;
    requestSurvey();
    sendOk(r);
  });

  webServer.on("/api/models", HTTP_GET, [](AsyncWebServerRequest *r) {
    AsyncResponseStream *resp = r->beginResponseStream("application/json");
    resp->print("[");
    for (int i = 0; i < CATALOG_MODEL_COUNT; i++)
    {
      JsonDocument item;
      item["name"] = CATALOG_MODELS[i].name;
      item["family"] = String(CATALOG_MODELS[i].family);
      item["protocol"] = String(CATALOG_MODELS[i].protocol);
      if (i)
        resp->print(",");
      serializeJson(item, *resp);
    }
    resp->print("]");
    r->send(resp);
  });

  webServer.on("/api/catalog", HTTP_GET, handleCatalog);

  webServer.on("/api/values", HTTP_GET, [](AsyncWebServerRequest *r) {
    AsyncResponseStream *resp = r->beginResponseStream("application/json");
    resp->print("[");
    valuesLock();
    for (size_t i = 0; i < converter.activeCount; i++)
    {
      const LabelDef &l = converter.activeLabels[i];
      JsonDocument item;
      item["l"] = l.label;
      item["v"] = l.asString;
      item["r"] = l.registryID;
      if (i)
        resp->print(",");
      serializeJson(item, *resp);
    }
    valuesUnlock();
    resp->print("]");
    r->send(resp);
  });

  // Event history (eventlog.h), newest first
  webServer.on("/api/events", HTTP_GET, [](AsyncWebServerRequest *r) {
    static EventRing copy; // too big for the stack; requests are served one at a time
    eventCopy(copy);
    JsonDocument doc;
    doc["boot"] = eventBoot;
    doc["uptime"] = millis() / 1000;
    doc["clock"] = eventClockSet;
    JsonArray list = doc["events"].to<JsonArray>();
    for (size_t i = copy.count(); i-- > 0;)
    {
      const EventEntry &e = copy.at(i);
      JsonObject item = list.add<JsonObject>();
      item["t"] = e.epoch;
      item["up"] = e.uptime;
      item["boot"] = e.boot;
      item["text"] = e.text;
    }
    sendJson(r, doc);
  });

  webServer.on("/api/wifi/scan", HTTP_GET, [](AsyncWebServerRequest *r) {
    int n = roamScanning ? WIFI_SCAN_RUNNING : WiFi.scanComplete(); // the roaming scan's results are not ours
    JsonDocument doc;
    if (n == WIFI_SCAN_FAILED)
    {
      WiFi.enableSTA(true); // off on Ethernet boards
      WiFi.scanNetworks(true);
      doc["running"] = true;
    }
    else if (n == WIFI_SCAN_RUNNING)
    {
      doc["running"] = true;
    }
    else
    {
      doc["running"] = false;
      JsonArray nets = doc["networks"].to<JsonArray>();
      for (int i = 0; i < n; i++)
      {
        JsonObject net = nets.add<JsonObject>();
        net["ssid"] = WiFi.SSID(i);
        net["rssi"] = WiFi.RSSI(i);
        net["secure"] = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
      }
      WiFi.scanDelete();
    }
    sendJson(r, doc);
  });

  webServer.on("/api/discover", HTTP_POST, [](AsyncWebServerRequest *r) {
    discoverDone = false;
    discoverRequested = true;
    sendOk(r);
  });

  webServer.on("/api/discover", HTTP_GET, [](AsyncWebServerRequest *r) {
    JsonDocument doc;
    doc["running"] = discoverRequested || discoverRunning;
    doc["done"] = discoverDone;
    JsonArray hosts = doc["hosts"].to<JsonArray>();
    for (int i = 0; i < discoveredCount; i++)
    {
      JsonObject h = hosts.add<JsonObject>();
      h["name"] = discovered[i].name;
      h["ip"] = discovered[i].ip;
      h["port"] = discovered[i].port;
      h["ha"] = discovered[i].homeAssistant;
      h["mqtt"] = discovered[i].mqtt;
    }
    sendJson(r, doc);
  });

  webServer.on("/api/mqtt/test", HTTP_POST, [](AsyncWebServerRequest *r) {
    if (!authorized(r))
      return;
    JsonDocument doc;
    if (!parseBody(r, doc))
      return;
    mqttTestConfig = config;
    configFromJson(mqttTestConfig, doc.as<JsonVariantConst>());
    strcpy(mqttTestResult, "running");
    mqttTestRequested = true;
    sendOk(r);
  }, nullptr, collectBody);

  webServer.on("/api/mqtt/test", HTTP_GET, [](AsyncWebServerRequest *r) {
    JsonDocument doc;
    doc["result"] = mqttTestResult;
    if (strcmp(mqttTestResult, "error") == 0)
    {
      doc["reason"] = mqttTestReason;
      doc["state"] = mqttTestState;
    }
    sendJson(r, doc);
  });

  webServer.on("/api/log", HTTP_GET, [](AsyncWebServerRequest *r) {
    char *text = (char *)malloc(LogRing::CAPACITY + 1);
    if (text == nullptr)
    {
      r->send(503);
      return;
    }
    logHistory.snapshot(text, LogRing::CAPACITY + 1);
    AsyncResponseStream *resp = r->beginResponseStream("text/plain");
    resp->print(text);
    free(text);
    r->send(resp);
  });

  webServer.on("/api/telemetry", HTTP_GET, [](AsyncWebServerRequest *r) {
    r->send(200, "application/json", telemetryPayload());
  });

  webServer.on("/api/reboot", HTTP_POST, [](AsyncWebServerRequest *r) {
    if (!authorized(r))
      return;
    rebootCause = "Restart requested from the web interface";
    rebootRequested = true;
    sendOk(r);
  });

  webServer.on("/api/factory-reset", HTTP_POST, [](AsyncWebServerRequest *r) {
    if (!authorized(r))
      return;
    factoryResetRequested = true;
    sendOk(r);
  });

  // Firmware update from the browser (the OTA .bin of a release)
  webServer.on("/update", HTTP_POST, [](AsyncWebServerRequest *r) {
    if (!authorized(r))
      return;
    bool ok = !updateFailed && !Update.hasError();
    r->send(ok ? 200 : 500, "application/json", ok ? "{\"ok\":true}" : "{\"ok\":false,\"error\":\"update failed\"}");
    if (ok)
    {
      rebootCause = "Firmware update from the web interface";
      rebootRequested = true;
    }
  }, [](AsyncWebServerRequest *r, String filename, size_t index, uint8_t *data, size_t len, bool final) {
    if (config.adminPwd[0] && !r->authenticate("admin", config.adminPwd))
      return;
    if (index == 0)
    {
      updateFailed = !Update.begin(UPDATE_SIZE_UNKNOWN);
      mqttSerial.printf("Firmware update started (%s)\n", filename.c_str());
      eventAddf("Firmware update from the web interface: %s", filename.c_str());
    }
    if (!updateFailed && Update.write(data, len) != len)
      updateFailed = true;
    if (final && !updateFailed)
    {
      updateFailed = !Update.end(true);
      mqttSerial.printf("Firmware update %s\n", updateFailed ? "failed" : "done, rebooting");
    }
  });

  // Captive portal: phones and laptops probe these URLs when joining the setup access point
  auto portal = [](AsyncWebServerRequest *r) {
    r->redirect("http://" + WiFi.softAPIP().toString() + "/");
  };
  webServer.on("/generate_204", HTTP_GET, portal);
  webServer.on("/hotspot-detect.html", HTTP_GET, portal);
  webServer.on("/connecttest.txt", HTTP_GET, portal);
  webServer.on("/ncsi.txt", HTTP_GET, portal);
  webServer.onNotFound([](AsyncWebServerRequest *r) {
    if (apActive && r->host() != WiFi.softAPIP().toString())
    {
      r->redirect("http://" + WiFi.softAPIP().toString() + "/");
      return;
    }
    r->send(404, "text/plain", "Not found");
  });

  webServer.addHandler(&webEvents);
  webServer.begin();
}

// Tests the MQTT settings with a separate connection. Blocks up to a few seconds: main loop only.
static void runMqttTest()
{
  LOOP_STAGE("MQTT test");
  mqttTestRequested = false;
  WiFiClient plain;
  PubSubClient test;
  test.setClient(plain);
  WiFiClientSecure secure;
  if (mqttTestConfig.mqttTls)
  {
    secure.setInsecure();
    test.setClient(secure);
  }
  test.setServer(mqttTestConfig.mqttServer, mqttTestConfig.mqttPort);
  char id[48];
  snprintf(id, sizeof(id), "%s-test", mqttTestConfig.mqttClientId);
  if (test.connect(id, mqttTestConfig.mqttUser, mqttTestConfig.mqttPwd))
  {
    strcpy(mqttTestResult, "ok");
    test.disconnect();
  }
  else
  {
    // PubSubClient states: -4 timeout, -2 connection failed, 4 bad credentials, 5 unauthorized
    mqttTestState = test.state();
    mqttTestReason = mqttTestState == -2 ? "broker unreachable" : (mqttTestState == 4 || mqttTestState == 5) ? "wrong user or password" : "connection refused";
    strcpy(mqttTestResult, "error");
  }
}

// What a report is about: the identification key of the heat pump and the model, hashed (FNV-1a). A board reports
// each heat pump + model once; re-confirming the same model sends nothing, a new unit or another model does.
static uint32_t telemetrySubject()
{
  char key[96];
  surveyKey(survey, key, sizeof(key));
  uint32_t h = 2166136261u;
  for (const char *p : {(const char *)key, "|", (const char *)config.model})
    for (; *p; p++)
      h = (h ^ (uint8_t)*p) * 16777619u;
  return h ? h : 1;
}

static void sendTelemetry()
{
  LOOP_STAGE("telemetry");
  telemetryPending = false;
  if (!config.telemetry || !config.modelConfirmed || !TELEMETRY_AVAILABLE || !netOnline() || !surveyJson[0] || surveyInProgress)
    return;
  uint32_t subject = telemetrySubject();
  WiFiClientSecure tls;
  tls.setInsecure();
  HTTPClient http;
  if (!http.begin(tls, TELEMETRY_URL))
    return;
  http.addHeader("Content-Type", "application/json");
  int code = http.POST(telemetryPayload());
  http.end();
  mqttSerial.printf("Telemetry sent (%d). Thank you!\n", code);
  // Done when accepted, or refused for good (4xx but 429): retried later otherwise
  if ((code >= 200 && code < 300) || (code >= 400 && code < 500 && code != 429))
  {
    config.telemetrySent = subject;
    configSave();
  }
}

// Sends the report when this heat pump + model has not been reported yet (eg. the box was ticked before the
// firmware could send, or a report failed). Once a minute at most, and 6 hours after a failed attempt.
static void telemetryLoop()
{
  static unsigned long lastCheck = 0, lastAttempt = 0;
  static bool attempted = false;
  if (millis() - lastCheck < 60000UL)
    return;
  lastCheck = millis();
  if (!config.telemetry || !config.modelConfirmed || !TELEMETRY_AVAILABLE || !netOnline() || !surveyJson[0] || surveyInProgress)
    return;
  if (telemetrySubject() == config.telemetrySent)
    return;
  if (attempted && millis() - lastAttempt < 6UL * 3600UL * 1000UL)
    return;
  attempted = true;
  lastAttempt = millis();
  sendTelemetry();
}

// Applies what the request handlers queued. Main loop only.
void webLoop()
{
  AppConfig *next = pendingConfig;
  if (next != nullptr)
  {
    LOOP_STAGE("apply settings");
    bool wifiChanged = strcmp(next->wifiSsid, config.wifiSsid) != 0 || strcmp(next->wifiPwd, config.wifiPwd) != 0 ||
                       next->staticIp != config.staticIp || next->ip != config.ip || next->gateway != config.gateway ||
                       next->subnet != config.subnet || next->dns1 != config.dns1 || next->dns2 != config.dns2;
    bool mqttChanged = strcmp(next->mqttServer, config.mqttServer) != 0 || next->mqttPort != config.mqttPort ||
                       strcmp(next->mqttUser, config.mqttUser) != 0 || strcmp(next->mqttPwd, config.mqttPwd) != 0 ||
                       next->mqttTls != config.mqttTls || strcmp(next->mqttClientId, config.mqttClientId) != 0 ||
                       next->jsonTable != config.jsonTable;
    bool labelsChanged = strcmp(next->model, config.model) != 0 || next->labelCount != config.labelCount ||
                         memcmp(next->labels, config.labels, sizeof(uint32_t) * next->labelCount) != 0 ||
                         next->fixCount != config.fixCount || next->layoutCheck != config.layoutCheck;
    labelsChanged = labelsChanged || strcmp(next->lang, config.lang) != 0;
    bool confirmedNow = next->modelConfirmed && (!config.modelConfirmed || labelsChanged);
    bool telemetryEnabled = next->telemetry && !config.telemetry;
    bool reboot = next->rxPin != config.rxPin || next->txPin != config.txPin || next->thermPin != config.thermPin ||
                  next->sg1Pin != config.sg1Pin || next->sg2Pin != config.sg2Pin || next->safetyPin != config.safetyPin ||
                  strcmp(next->hostname, config.hostname) != 0 || strcmp(next->adminPwd, config.adminPwd) != 0;
    config = *next;
    pendingConfig = nullptr;
    delete next;
    configSave();
    mqttSerial.println("Configuration saved.");
    if (wifiChanged)
      connectWifi();
    if (mqttChanged)
      mqttReconnectNow();
    if (labelsChanged)
      labelsDirty = true;
    if (confirmedNow || telemetryEnabled)
      telemetryPending = true;
    if (reboot)
    {
      rebootCause = "Settings changed from the web interface";
      rebootRequested = true;
    }
  }

  if (discoverRequested)
    runDiscovery();
  if (mqttTestRequested)
    runMqttTest();
  if (telemetryPending)
    sendTelemetry();
  telemetryLoop();

  if (factoryResetRequested)
  {
    configReset();
    rebootCause = "Factory reset from the web interface";
    rebootRequested = true;
    factoryResetRequested = false;
  }
  if (rebootRequested)
  {
    if (rebootAt == 0)
      rebootAt = millis() + 1000; // lets the HTTP response go out
    else if ((long)(millis() - rebootAt) >= 0)
    {
      mqttSerial.println("Rebooting...");
      restart_board(rebootCause);
    }
  }

  static unsigned long lastStatus = 0;
  if (webEvents.count() > 0 && millis() - lastStatus > 5000)
  {
    lastStatus = millis();
    JsonDocument doc;
    statusJson(doc);
    String s;
    serializeJson(doc, s);
    webEvents.send(s.c_str(), "status");
  }
}

#endif
