#if defined(ARDUINO_M5Stick_C_Plus2) || defined(ARDUINO_M5Stick_C_Plus) || defined(ARDUINO_M5Stick_C) || defined(ARDUINO_M5Stack_Tough)
#include <M5Unified.h>
#else
#include <Arduino.h>
#endif

#ifdef ARDUINO_ARCH_ESP8266
#include <SoftwareSerial.h>
#include <ESP8266WiFi.h>
#else
#include <WiFi.h>
#endif
#include <HardwareSerial.h>

#include <PubSubClient.h>
#include <ArduinoOTA.h>

#if defined(ESPALTHERMA_GENERIC)
// Generic firmware (web flasher): no compile time settings, everything is configured at runtime.
#elif __has_include("my_setup.h")
#include "my_setup.h"
#else
#include "setup.h"
#endif

#ifdef LABELDEF
// A definition file was included by the setup: its uncommented values are the ones queried.
#define HAS_STATIC_LABELS
#endif
#include "labeldef.h"

#include "board.h"
#include "version.h"
#include "config.h"
#include "mqttserial.h"
#include "converters.h"
#include "comm.h"
#include "survey.h"
#ifdef HAS_CATALOG
#include "catalog.h"
#include "detect.h"
#include "layoutfix.h"
#endif
#include "homeassistant.h"
#include "mqtt.h"
#include "restart.h"
#include "power.h"
#ifdef HAS_WEBUI
#include "netmgr.h"
#include "improv.h"
#include "webserver.h"
#endif

::Converter converter; // qualified: ArduinoJson (via ESPAsyncWebServer) also has a Converter
char registryIDs[32]; //Holds the registries to query
bool registryOk[32]; //Registries successfully read during the last query cycle
bool busy = false;

Survey survey;
char surveyJson[3072];
unsigned long lastSurvey = 0;
volatile bool discoveryDirty = false; //the values changed: HA discovery must be sent again
volatile bool labelsDirty = false;    //the model or the selection changed: the heat pump task reloads the values
volatile bool forceDetection = false; //detect again even if a model is set (requested from the web interface)
unsigned long lastPollMs = 0;         //end of the last query cycle
#ifdef HAS_CATALOG
DetectResult detection;
std::vector<LabelDef> catalogLabels; //values of the model, generic firmware
LayoutChecker layoutChecker;         //checks the model's layout against the replies (layoutfix.h)
LayoutCycle layoutCycle;             //replies of the last query cycle
#endif
volatile bool surveyRequested = false;
volatile bool surveyReady = false;

#ifdef HAS_HP_TASK
// The heat pump task converts values while the main loop publishes them.
SemaphoreHandle_t valuesMutex = xSemaphoreCreateMutex();
void valuesLock() { xSemaphoreTake(valuesMutex, portMAX_DELAY); }
void valuesUnlock() { xSemaphoreGive(valuesMutex); }
volatile bool cycleReady = false;
#else
void valuesLock() {}
void valuesUnlock() {}
#endif

#if defined(ARDUINO_M5Stick_C_Plus2) || defined(ARDUINO_M5Stick_C_Plus) || defined(ARDUINO_M5Stick_C) || defined(ARDUINO_M5Stack_Tough)
#define HAS_M5_SCREEN
long LCDTimeout = 40000;//Keep screen ON for 40s then turn off. ButtonA will turn it On again.
#endif

bool contains(char array[], int size, int value)
{
  for (int i = 0; i < size; i++)
  {
    if (array[i] == value)
      return true;
  }
  return false;
}

//Converts to string and add the value to the JSON message
void updateValues(char regID)
{
  LabelDef *labels[128];
  int num = 0;
  converter.getLabels(regID, labels, num);
  for (int i = 0; i < num; i++)
  {
    bool alpha = false;
    for (size_t j = 0; j < strlen(labels[i]->asString); j++)
    {
      char c = labels[i]->asString[j];
      if (!isdigit(c) && c!='.' && !(c=='-' && j==0)){
        alpha = true;
        break;
      }
    }

    if (config.oneValOneTopic)
    {
      char topicBuff[128];
      snprintf(topicBuff, sizeof(topicBuff), "%s%s", config.oneTopicPrefix, labels[i]->label);
      client.publish(topicBuff, labels[i]->asString);
    }
    else if (alpha){

      snprintf(jsonbuff + strlen(jsonbuff), MAX_MSG_SIZE - strlen(jsonbuff), "\"%s\":\"%s\",", labels[i]->label, labels[i]->asString);
    }
    else{//number, no quotes
      snprintf(jsonbuff + strlen(jsonbuff), MAX_MSG_SIZE - strlen(jsonbuff), "\"%s\":%s,", labels[i]->label, labels[i]->asString);
    }
  }
}

uint16_t loopcount =0;
boolean display_sleeping = false;

#ifdef HAS_M5_SCREEN
#ifdef ARDUINO_M5Stack_Tough
#define SCREEN_BRIGHTNESS 12
#else
#define SCREEN_BRIGHTNESS 100
#endif

void wakeScreen()
{
  M5.Display.wakeup();
  M5.Display.setBrightness(SCREEN_BRIGHTNESS);
  LCDTimeout = millis() + 30000;
  display_sleeping = false;
}

//Non blocking: also called while waiting for WiFi/MQTT so the button
//can wake the screen even when there is no connection.
void handleScreen()
{
  M5.update();
#ifdef ARDUINO_M5Stack_Tough
  bool wakeRequested = M5.Touch.getCount() > 0 && M5.Touch.getDetail().wasPressed();
#else
  bool wakeRequested = M5.BtnA.wasPressed();
#endif
  if (wakeRequested){//Turn back ON screen
    wakeScreen();
  } else if (LCDTimeout < millis() && !display_sleeping) { //Turn screen off.
    M5.Display.setBrightness(0);
    M5.Display.sleep();
    display_sleeping = true;
  }
}
#else
void wakeScreen(){}
void handleScreen(){}
#endif

void extraLoop()
{
  client.loop();
  ArduinoOTA.handle();
  while (busy)
  { //Stop processing during OTA
    ArduinoOTA.handle();
  }
  handleScreen();
  samplePower();
  mqttSerial.drain();
}

#ifdef ARDUINO_ARCH_ESP8266
void get_wifi_bssid(const char *ssid, uint8_t *bssid, uint32_t *wifi_channel)
{
  bssid = nullptr;
  int n = WiFi.scanNetworks(false, true);

  if (n < 1) // no networks found
    return;

  // sort networks on RSSI value
  int indices[n];
  for (int i = 0; i < n; i++)
  {
    indices[i] = i;
  }

  for (int i = 0; i < n; i++)
  {
    for (int j = i + 1; j < n; j++)
    {
      if (WiFi.RSSI(indices[j]) > WiFi.RSSI(indices[i]))
      {
        std::swap(indices[i], indices[j]);
      }
    }
  }

  // loop through result and match highest RSSI SSID
  for (int i = 0; i < n; i++)
  {
    char scan_ssid[33]; // ssid can be up to 32chars, => plus null term
    strlcpy(scan_ssid, WiFi.SSID(indices[i]).c_str(), sizeof(scan_ssid));

    if (strcmp(ssid, scan_ssid) == 0)
    {
      if (WiFi.BSSID(indices[i]) != 0)
      {
        bssid = WiFi.BSSID(indices[i]);
        *wifi_channel = WiFi.channel();
        return;
      }
      else
      {
        return;
      }
    }
  }
}
#endif

void checkWifi()
{
  if (WiFi.status() == WL_CONNECTED)
    return;

  if (config.wifiSsid[0] == 0)
  {
    mqttSerial.println("No WiFi network configured.");
    while (true)
    { //Nothing to connect to: wait for provisioning
      handleScreen();
      mqttSerial.drain();
      delay(50);
    }
  }

  unsigned long lostTime = millis();
  unsigned long lastAttempt = millis();
  unsigned long lastDot = 0;
  wakeScreen();//Show we lost connection; keep the screen usable during the outage
  while (WiFi.status() != WL_CONNECTED)
  {
    handleScreen();//Keep the button responsive while disconnected
    delay(50);
    if (millis() - lastDot >= 500)
    {
      Serial.print(".");
      lastDot = millis();
    }
    if (millis() - lastAttempt >= 15000)
    { //Auto-reconnect is not making it: force a full scan so we reattach to the strongest AP
      Serial.println("\nStill disconnected. Rescanning for strongest AP...");
      WiFi.disconnect();
      delay(100);
      WiFi.begin(config.wifiSsid, config.wifiPwd, 0, 0, true);
      lastAttempt = millis();
    }
    if (millis() - lostTime >= 120000)
    { //Still no WiFi after 2 min: reboot in case the WiFi stack is wedged
      Serial.printf("Tried connecting for 120 sec, rebooting now.");
      restart_board();
    }
  }
}

#ifndef ARDUINO_ARCH_ESP8266
//With several APs sharing the same SSID, the ESP32 stays associated to its AP
//until the link fully drops, even if a much closer AP is available. Periodically
//check the signal and reattach to a significantly stronger AP of the same SSID.
#define ROAM_CHECK_INTERVAL 60000UL
#define ROAM_RSSI_THRESHOLD -75 //Only consider roaming when weaker than this
#define ROAM_MIN_IMPROVEMENT 8  //dB gain required to switch AP
unsigned long lastRoamCheck = 0;

void checkWifiRoaming()
{
  if (WiFi.status() != WL_CONNECTED || millis() - lastRoamCheck < ROAM_CHECK_INTERVAL)
    return;
  lastRoamCheck = millis();

  int32_t currentRSSI = WiFi.RSSI();
  if (currentRSSI >= ROAM_RSSI_THRESHOLD)
    return;

  int16_t n = WiFi.scanNetworks(false, true);
  int16_t bestIndex = -1;
  int32_t bestRSSI = currentRSSI + ROAM_MIN_IMPROVEMENT;
  for (int16_t i = 0; i < n; i++)
  {
    if (WiFi.SSID(i) == config.wifiSsid && WiFi.RSSI(i) > bestRSSI
        && memcmp(WiFi.BSSID(i), WiFi.BSSID(), 6) != 0)
    {
      bestRSSI = WiFi.RSSI(i);
      bestIndex = i;
    }
  }
  if (bestIndex >= 0)
  {
    uint8_t bssid[6];
    memcpy(bssid, WiFi.BSSID(bestIndex), 6);
    int32_t channel = WiFi.channel(bestIndex);
    mqttSerial.printf("WiFi weak (%ddBm), roaming to stronger AP (%ddBm, ch%d)\n", currentRSSI, bestRSSI, channel);
    WiFi.scanDelete();
    WiFi.disconnect();
    WiFi.begin(config.wifiSsid, config.wifiPwd, channel, bssid, true);
#ifndef HAS_WEBUI
    checkWifi();
#endif
  }
  else
  {
    WiFi.scanDelete();
  }
}
#else
void checkWifiRoaming(){}
#endif

void setup_wifi()
{
  delay(10);
  // We start by connecting to a WiFi network
  mqttSerial.printf("Connecting to %s\n", config.wifiSsid);

  if (config.staticIp)
  {
    if (!WiFi.config(IPAddress(config.ip), IPAddress(config.gateway), IPAddress(config.subnet), IPAddress(config.dns1), IPAddress(config.dns2))) {
      mqttSerial.println("Failed to set static ip!");
    }
  }

  uint8_t *bssid = nullptr;
  uint32_t wifi_channel = 0;
#ifdef ARDUINO_ARCH_ESP8266
  WiFi.hostname(config.hostname);
  get_wifi_bssid(config.wifiSsid, bssid, &wifi_channel);
#else //assume ESP32
  WiFi.setHostname(config.hostname);
  WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);
  WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
#endif
    
  if (bssid != nullptr)
  {
    WiFi.begin(config.wifiSsid, config.wifiPwd, wifi_channel, bssid);
  }
  else
  {
    WiFi.begin(config.wifiSsid, config.wifiPwd, 0, 0, true);
  }
  WiFi.setAutoReconnect(true);
  checkWifi();
  mqttSerial.printf("Connected. IP Address: %s\n", WiFi.localIP().toString().c_str());
}

void initLabels(){
#ifdef HAS_STATIC_LABELS
  converter.setLabels(labelDefs, sizeof(labelDefs) / sizeof(LabelDef));
#elif defined(HAS_CATALOG)
  //Generic firmware: the values of the configured model, from the catalog
  int model = catalogFindModel(config.model);
  if (model < 0)
  {
    catalogLabels.clear();
    converter.setLabels(nullptr, 0);
    return;
  }
  config.protocol = CATALOG_MODELS[model].protocol;
  //Registries corrected with the layout of another definition
  CatalogFix fixes[CONFIG_MAX_FIXES];
  size_t fixCount = 0;
  for (uint8_t f = 0; f < config.fixCount; f++)
  {
    int fixModel = catalogFindModel(config.fixModel[f]);
    if (fixModel >= 0)
    {
      fixes[fixCount++] = {config.fixReg[f], fixModel};
      mqttSerial.printf("Registry 0x%02x read with the layout of %s\n", config.fixReg[f], config.fixModel[f]);
    }
  }
  //New model, or corrections undone: the evidence starts again
  static uint8_t lastFixCount = 0;
  static bool lastLayoutCheck = true;
  if (layoutChecker.model != model || layoutChecker.count == 0 || config.fixCount < lastFixCount || (config.layoutCheck && !lastLayoutCheck))
  {
    layoutCheckInit(layoutChecker, model);
  }
  lastFixCount = config.fixCount;
  lastLayoutCheck = config.layoutCheck;
  int refrigerant = catalogBuildLabels(model, config.labels, config.labelCount, catalogLabels, fixes, fixCount);
  converter.RType = refrigerant ? refrigerant : 802;
  converter.setLabels(catalogLabels.data(), catalogLabels.size());
  mqttSerial.printf("Model: %s (%s), %d values\n", config.model, config.modelConfirmed ? "confirmed" : "to be confirmed", (int)catalogLabels.size());
#endif
}

void initRegistries(){
    //getting the list of registries to query from the selected values
  for (size_t i = 0; i < sizeof(registryIDs); i++)
  {
    registryIDs[i]=0xff;
  }

  int i = 0;
  for (size_t l = 0; l < converter.activeCount; l++)
  {
    LabelDef &label = converter.activeLabels[l];
    if (!contains(registryIDs, sizeof(registryIDs), label.registryID))
    {
      mqttSerial.printf("Adding registry 0x%2x to be queried.\n", label.registryID);
      registryIDs[i++] = label.registryID;
    }
  }
  if (i == 0)
  {
#ifdef HAS_STATIC_LABELS
    mqttSerial.printf("ERROR - No values selected in the include file. Stopping.\n");
    while (true)
    {
      extraLoop();
    }
#else
    mqttSerial.printf("No values selected yet.\n");
#endif
  }
}

void setupScreen(){
#if !defined(ARDUINO_M5Stick_C_Plus2) && defined(ARDUINO_M5Stick_C) || defined(ARDUINO_M5Stick_C_Plus) || defined(ARDUINO_M5Stack_Tough)
  M5.begin();
  M5.Lcd.setRotation(1);
  M5.Display.setBrightness(127);
  M5.Lcd.fillScreen(TFT_WHITE);
  M5.Lcd.setFreeFont(&FreeSansBold12pt7b);
  M5.Lcd.setTextDatum(MC_DATUM);
  int xpos = M5.Lcd.width() / 2; // Half the screen width
  int ypos = M5.Lcd.height() / 2; // Half the screen width
  M5.Lcd.setTextColor(TFT_DARKGREY);
  M5.Lcd.drawString("ESPAltherma", xpos,ypos);
  delay(2000);
  M5.Lcd.fillScreen(TFT_BLACK);
  M5.Lcd.setTextFont(1);
  M5.Lcd.setTextColor(TFT_GREEN);

#elif defined(ARDUINO_M5Stick_C_Plus2)
  M5.begin();
  M5.Lcd.setRotation(1);
  M5.Lcd.setBrightness(127);
  M5.Lcd.fillScreen(TFT_WHITE);
  M5.Lcd.setFont(&FreeSansBold12pt7b);
  M5.Lcd.setTextDatum(MC_DATUM);
  int xpos = M5.Lcd.width() / 2; // Half the screen width
  int ypos = M5.Lcd.height() / 2; // Half the screen width
  M5.Lcd.setTextColor(TFT_DARKGREY);
  M5.Lcd.drawString("ESPAltherma", xpos,ypos);
  delay(2000);
  M5.Lcd.fillScreen(TFT_BLACK);
  M5.Lcd.setFont(&Font0);
  M5.Lcd.setTextColor(TFT_GREEN);
#endif



}

// Query function used by the survey: same as the polling, on a zeroed buffer.
bool surveyQuery(uint8_t regID, unsigned char *buffer, char protocol)
{
  memset(buffer, 0, SURVEY_BUFFER_SIZE);
  return queryRegistry(regID, buffer, protocol);
}

void requestSurvey()
{
  surveyRequested = true;
}

void valuesLock();
void valuesUnlock();

// Rebuilds the values to query after a model or selection change. Heat pump task (or setup) only.
void reloadLabels()
{
  valuesLock();
  initLabels();
  initRegistries();
  memset(registryOk, 0, sizeof(registryOk));
  valuesUnlock();
  discoveryDirty = true;
}

#ifdef HAS_CATALOG
#ifdef HAS_STATIC_LABELS
// Models whose definition has every value compiled in. The user knows their definition works:
// reported next to the fingerprint, this is the ground truth that grows the fingerprint table.
void appendConfiguredModels(char *out, size_t size, size_t pos)
{
  const size_t count = sizeof(labelDefs) / sizeof(LabelDef);
  pos--; //reopen the detection object
  surveyAppend(out, size, pos, ",\"configured\":[");
  int matches = 0;
  for (int m = 0; m < CATALOG_MODEL_COUNT; m++)
  {
    bool all = count > 0;
    for (size_t l = 0; l < count && all; l++)
    {
      const LabelDef &d = labelDefs[l];
      bool found = false;
      for (int i = 0; i < CATALOG_ENTRY_COUNT && !found; i++)
      {
        const CatalogEntry &e = CATALOG_ENTRIES[i];
        found = catalogInModel(e, m) && e.reg == d.registryID && e.offset == d.offset && e.conv == d.convid && e.size == d.dataSize;
      }
      all = found;
    }
    if (all && matches++ < 6)
      surveyAppend(out, size, pos, "%s\"%s\"", matches > 1 ? "," : "", CATALOG_MODELS[m].name);
  }
  surveyAppend(out, size, pos, "],\"configured_count\":%d}", matches);
}
#else
// Generic firmware: uses the detected model, unless a model is already set for this heat pump.
void applyDetection()
{
  if (detection.model < 0)
  {
    mqttSerial.println("No heat pump answered, check the connection to X10A.");
    return;
  }
  char key[sizeof(config.detectedKey)];
  surveyKey(survey, key, sizeof(key));
  bool sameHeatPump = strcmp(key, config.detectedKey) == 0;
  bool forced = forceDetection;
  forceDetection = false;
  if (config.model[0] && sameHeatPump && !forced)
    return;
  if (config.model[0])
    mqttSerial.println("A different heat pump answered: detecting its model again.");

  uint32_t keys[CONFIG_MAX_LABELS];
  int safe = detectSafeKeys(detection, keys, CONFIG_MAX_LABELS);
  if (safe == 0)
  {
    mqttSerial.println("Model uncertain and no value is safe to read: choose the model in the web interface.");
    return;
  }
  strlcpy(config.model, CATALOG_MODELS[detection.model].name, sizeof(config.model));
  strlcpy(config.detectedKey, key, sizeof(config.detectedKey));
  config.fixCount = 0; //corrections belong to the previous model
  config.modelConfirmed = detection.confidence == DETECT_HIGH;
  config.labelCount = safe < 0 ? 0 : safe; //0: all the recommended values
  memcpy(config.labels, keys, sizeof(uint32_t) * config.labelCount);
  configSave();
  mqttSerial.printf("Detected %s (%s confidence)\n", config.model, detectConfidenceName(detection.confidence));
  reloadLabels();
}

// Scores the model's layout on the last replies, and corrects a registry that clearly does not match the unit.
// Heat pump task only (reloads the values).
void layoutCheck()
{
  if (!config.layoutCheck || layoutCycle.count == 0 || layoutChecker.count == 0)
    return;
  layoutCheckCycle(layoutChecker, layoutCycle);
  bool changed = false;
  for (uint8_t i = 0; i < layoutChecker.count; i++)
  {
    LayoutCheck &rc = layoutChecker.regs[i];
    bool fixed = false;
    for (uint8_t f = 0; f < config.fixCount && !fixed; f++)
      fixed = config.fixReg[f] == rc.reg;
    int k = fixed ? -1 : layoutCheckDecide(rc);
    if (k < 0 || config.fixCount >= CONFIG_MAX_FIXES)
      continue;
    const LayoutCandidate &a = rc.c[0], &c = rc.c[k];
    mqttSerial.printf("Registry 0x%02x does not match %s on this unit (%u/%u plausible values): read with the layout of %s (%u/%u)\n",
                      rc.reg, config.model, a.plausible, a.checks, CATALOG_MODELS[c.model].name, c.plausible, c.checks);
    config.fixReg[config.fixCount] = rc.reg;
    strlcpy(config.fixModel[config.fixCount], CATALOG_MODELS[c.model].name, sizeof(config.fixModel[0]));
    config.fixCount++;
    changed = true;
  }
  if (changed)
  {
    configSave();
    reloadLabels();
  }
}

// The survey replies count as a first poll cycle for the layout check.
void layoutFeedSurvey()
{
  layoutCycle.clear();
  for (uint8_t i = 0; i < survey.count; i++)
  {
    const SurveyReg &r = survey.regs[i];
    if (r.answered)
      layoutCycle.add(r.id, r.payload, r.len);
  }
  layoutCheck();
}
#endif
#endif

// Reads every known registry once and builds the detection report. Runs where the registries are polled.
void runSurvey()
{
  surveyRequested = false;
  lastSurvey = millis();
  mqttSerial.println("Starting heat pump survey...");
  surveyRun(survey, surveyQuery);
  const char *detectJson = nullptr;
#ifdef HAS_CATALOG
  static char detectBuf[1024];
  detectModel(survey, detection);
  size_t len = detectToJson(detection, detectBuf, sizeof(detectBuf));
#ifdef HAS_STATIC_LABELS
  appendConfiguredModels(detectBuf, sizeof(detectBuf), len);
#endif
  detectJson = detectBuf;
#endif
  //Rendered aside then swapped in: the web interface may be reading the previous report
  static char report[sizeof(surveyJson)];
  surveyToJson(survey, report, sizeof(report), ESPALTHERMA_VERSION, BOARD_NAME, detectJson);
  valuesLock();
  memcpy(surveyJson, report, sizeof(surveyJson));
  valuesUnlock();
  mqttSerial.printf("Survey done: protocol %c, %d registries.\n", survey.protocol ? survey.protocol : '-', survey.count);
#if defined(HAS_CATALOG) && !defined(HAS_STATIC_LABELS)
  applyDetection();
  if (config.model[0] && survey.protocol == 'I')
    layoutFeedSurvey();
#endif
  surveyReady = true;
}

void publishSurvey()
{
  surveyReady = false;
  Serial.println(surveyJson);
  client.publish(MQTT_detect, surveyJson, true);
}

void hpDelay(unsigned long ms);

// Queries all registries and converts their values.
// Runs in the heat pump task (ESP32) or in the main loop (ESP8266).
void layoutCheck();

void pollRegistries()
{
#if defined(HAS_CATALOG) && !defined(HAS_STATIC_LABELS)
  layoutCycle.clear();
#endif
  for (size_t i = 0; (i < 32) && (uint8_t)registryIDs[i] != 0xFF; i++)
  {
    unsigned char buff[REPLY_BUFFER_SIZE] = {0};
    int tries = 0;
    while (!queryRegistry(registryIDs[i], buff, config.protocol) && tries++ < 3)
    {
      mqttSerial.println("Retrying...");
      hpDelay(1000);
    }
    unsigned char receivedRegistryID = config.protocol == 'S' ? buff[0] : buff[1];
    registryOk[i] = (uint8_t)registryIDs[i] == receivedRegistryID; //if replied registerID is coherent with the command
    if (registryOk[i])
    {
      valuesLock();
      converter.readRegistryValues(buff, config.protocol); //process all values from the register
#ifdef HAS_WEBUI
      if (config.protocol == 'I')
        replyCache.update(buff[1], buff + 3, buff[2] >= 2 ? buff[2] - 2 : 0);
      else
        replyCache.update(buff[0], buff + 1, get_reply_len(buff[0], 'S') - 2);
#endif
      valuesUnlock();
#if defined(HAS_CATALOG) && !defined(HAS_STATIC_LABELS)
      if (config.protocol == 'I' && buff[2] >= 2)
        layoutCycle.add(buff[1], buff + 3, buff[2] - 2);
#endif
    }
  }
#if defined(HAS_CATALOG) && !defined(HAS_STATIC_LABELS)
  layoutCheck();
#endif
}

// Sends the values of the last query cycle in mqtt. Main loop only.
void publishValues()
{
  valuesLock();
  for (size_t i = 0; (i < 32) && (uint8_t)registryIDs[i] != 0xFF; i++)
  {
    if (registryOk[i])
    {
      updateValues(registryIDs[i]);
    }
  }
  valuesUnlock();
  sendValues();//Send the full json message
#ifdef HAS_WEBUI
  webSendValues();
#endif
}

#ifdef HAS_HP_TASK
void hpDelay(unsigned long ms)
{
  delay(ms);
}

// Owns the X10A link: runs the survey on request and queries the registries every config.frequency ms.
void hpTask(void *)
{
  for (;;)
  {
    unsigned long start = millis();
#if defined(HAS_CATALOG) && !defined(HAS_STATIC_LABELS)
    if (!config.model[0] && millis() - lastSurvey > 60000)
    { //No model yet: the heat pump may not have been connected, try again
      surveyRequested = true;
    }
#endif
    if (labelsDirty)
    {
      labelsDirty = false;
      reloadLabels();
    }
    if (surveyRequested)
    {
      runSurvey();
    }
    if (!busy && (uint8_t)registryIDs[0] != 0xFF)
    {
      pollRegistries();
      lastPollMs = millis();
      cycleReady = true;
      mqttSerial.printf("Done. Waiting %ld ms...", (long)(config.frequency - (millis() - start)));
    }
    while (millis() - start < config.frequency && !surveyRequested && !labelsDirty)
    {
      delay(100);
    }
  }
}
#else
void waitLoop(uint ms);
void hpDelay(unsigned long ms)
{
  waitLoop(ms);
}
#endif

void setupPins()
{
  if (config.thermPin >= 0)
  {
    pinMode(config.thermPin, OUTPUT);
  }

  if (config.safetyPin >= 0)
  {
    //Inactive level first, then output: an output starts low, which would be active for a low-triggered relay
    digitalWrite(config.safetyPin, !safetyActiveState());
    pinMode(config.safetyPin, OUTPUT);
  }

  if (config.sg1Pin >= 0 && config.sg2Pin >= 0)
  {
    //Smartgrid pins - Set first to the inactive state, before configuring as outputs (avoid false triggering when initializing)
    digitalWrite(config.sg1Pin, sgInactiveState());
    digitalWrite(config.sg2Pin, sgInactiveState());
    pinMode(config.sg1Pin, OUTPUT);
    pinMode(config.sg2Pin, OUTPUT);
  }
}

void setup()
{
  Serial.begin(115200);
#if ARDUINO_USB_CDC_ON_BOOT && ARDUINO_USB_MODE
  Serial.setTxTimeoutMs(0); //USB port of the chip: do not block when no computer is connected
#endif
#ifdef SECOND_CONSOLE
  SECOND_CONSOLE.begin(115200);
#endif
  configLoad();
  setupPins(); //relays to their inactive state first: they would float during the splash screen
  setupScreen();
  MySerial.begin(9600, SERIAL_CONFIG, config.rxPin, config.txPin);
#ifdef ARDUINO_M5Stick_C_Plus
  gpio_pulldown_dis(GPIO_NUM_25);
  gpio_pullup_dis(GPIO_NUM_25);
#endif

  EEPROM.begin(10);
  readEEPROM();//Restore previous state
  ArduinoOTA.onStart([]() {
    busy = true;
  });

  ArduinoOTA.onError([](ota_error_t error) {
    mqttSerial.print("Error on OTA - restarting");
    restart_board();
  });

  setupMqttClient();
  client.setCallback(callback);
  mqttSerial.begin(&client, "espaltherma/log");

#ifdef HAS_WEBUI
  //Nothing blocks: WiFi, the setup access point and MQTT are handled by netLoop()
  netBegin();
  webBegin();
#else
  mqttSerial.print("Setting up wifi...");
  setup_wifi();
  ArduinoOTA.setHostname(config.hostname);
  ArduinoOTA.begin();

  mqttSerial.printf("Connecting to MQTT server: %s:%d\n", config.mqttServer, config.mqttPort);
  reconnectMqtt();
  mqttSerial.println("OK!");
#endif

  initLabels();
  initRegistries();
#if defined(ESPALTHERMA_GENERIC) || defined(SURVEY_ON_BOOT)
  requestSurvey();
#endif
#ifdef HAS_HP_TASK
  xTaskCreate(hpTask, "heatpump", 8192, nullptr, 1, nullptr);
#endif
  mqttSerial.print("ESPAltherma started!");
#ifdef ARDUINO_ARCH_ESP32
  mqttSerial.printf(" Last reset: %s\n", resetReasonName());
#endif
}

void waitLoop(uint ms){
      unsigned long start = millis();
      while (millis() < start + ms) //wait .5sec between registries
      {
        extraLoop();
      }
}

void loop()
{
  unsigned long start = millis();
#ifdef HAS_WEBUI
  netLoop();
  improv.loop();
#ifdef SECOND_CONSOLE
  improv2.loop();
#endif
  webLoop();
#else
  if (WiFi.status() != WL_CONNECTED)
  { //restart board if needed
    checkWifi();
  }
  checkWifiRoaming();//Move to a stronger AP of the same SSID if signal got weak
  if (!client.connected())
  { //(re)connect to MQTT if needed
    reconnectMqtt();
  }
#endif
#ifdef HAS_HP_TASK
  //The heat pump task does the querying, we publish its results
  extraLoop();
  if (cycleReady)
  {
    cycleReady = false;
    publishValues();
  }
  if (surveyReady && client.connected())
  {
    publishSurvey();
  }
  if (discoveryDirty && client.connected())
  {
    discoveryDirty = false;
    publishHomeAssistantDeviceDiscovery();
  }
  delay(5);
#else
  if (surveyRequested)
  {
    runSurvey();
    publishSurvey();
  }
  //Querying all registries
  pollRegistries();
  publishValues();
  unsigned long elapsed = millis() - start;
  unsigned long wait = elapsed < config.frequency ? config.frequency - elapsed : 0;
  mqttSerial.printf("Done. Waiting %ld ms...", (long)wait);
  waitLoop(wait);
#endif
}
