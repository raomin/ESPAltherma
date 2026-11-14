#if defined(ARDUINO_M5Stick_C_Plus2) || defined(ARDUINO_M5Stick_C_Plus) || defined(ARDUINO_M5Stick_C) || defined(ARDUINO_M5Stack_Tough)
#include <M5Unified.h>
#else
#include <Arduino.h>
#endif

#include <WiFi.h>
#include <HardwareSerial.h>

#include <PubSubClient.h>
#include <ArduinoOTA.h>
#include <atomic>

// No compile time settings: everything is configured at runtime (web interface, stored in NVS).
#include "labeldef.h"

#include "board.h"
#include "heapmark.h"
#include "version.h"
#include "config.h"
#include "mqttserial.h"
#include "converters.h"
#include "comm.h"
#include "registrypoll.h"
#include "control.h"
#include "diii_comm.h"
#include "stats.h"
#include "survey.h"
#include "catalog.h"
#include "detect.h"
#include "layoutfix.h"
#include "homeassistant.h"
#include "mqtt.h"
#include "restart.h"
#include "power.h"
#include "watchdog.h"
#include "netmgr.h"
#include "improv.h"
#include "webserver.h"

::Converter converter; // qualified: ArduinoJson (via ESPAsyncWebServer) also has a Converter
char registryIDs[32]; //Holds the registries to query
bool registryOk[32]; //Registries successfully read during the last query cycle
std::atomic<bool> busy{false}; // OTA/main task publishes the gate; the heat-pump task observes it

Survey survey;
char surveyJson[3072];
bool surveyReportFailed = false; // read/written under valuesLock, no partial report is published
unsigned long lastSurvey = 0;
volatile bool discoveryDirty = false; //the values changed: HA discovery must be sent again
volatile bool labelsDirty = false;    //the model or the selection changed: the heat pump task reloads the values
volatile bool forceDetection = false; //detect again even if a model is set (requested from the web interface)
unsigned long lastPollMs = 0;         //end of the last query cycle
DetectResult detection;
std::vector<LabelDef> catalogLabels; //values of the model, from the catalog
LayoutChecker layoutChecker;         //checks the model's layout against the replies (layoutfix.h)
LayoutCycle layoutCycle;             //replies of the last query cycle
volatile bool surveyRequested = false;
volatile bool surveyInProgress = false; //the survey is being written: readers must not use it
volatile bool surveyReady = false;
CurveControl dhwControl, lwtControl;   //experimental hot water and leaving water targets (control.h), heat pump task
LimitControl limitControl;             //experimental power limit (control.h), heat pump task
MeterReader unitMeter; // the unit's own energy meter, where the hydro board is mapped for it (control.h)
DiiiComm diii;                     //read-only DIII E0/63 frames of an HT outdoor unit (diii_comm.h), heat pump task
static void diiiSleep(unsigned long ms)
{
  watchdogHp();
  delay(ms);
}
static void diiiLog(const char *s) { mqttSerial.print(s); }

// The heat pump task converts values while the main loop publishes them.
SemaphoreHandle_t valuesMutex = xSemaphoreCreateMutex();
void valuesLock() { xSemaphoreTake(valuesMutex, portMAX_DELAY); }
void valuesUnlock() { xSemaphoreGive(valuesMutex); }
volatile bool cycleReady = false;

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
  LOOP_STAGE("MQTT receive");
  client.loop();
  LOOP_STAGE("ArduinoOTA");
  ArduinoOTA.handle();
  while (busy)
  { //Stop processing during OTA
    ArduinoOTA.handle();
  }
  LOOP_STAGE("screen");
  handleScreen();
  LOOP_STAGE("power");
  samplePower();
  LOOP_STAGE("log to MQTT");
  mqttSerial.drain();
  LOOP_STAGE("event history");
  eventLoop();
}

// Two zones: the model's mixed water temperature (the bizone kit's, or the unit's own R7T) reads something in the
// survey; a single-zone unit answers 0
static bool surveyTwoZones(int model)
{
  if (model < 0 || surveyInProgress)
    return false;
  catalogHpsu() = catalogModelHpsu(model);
  static ::Converter decoder; // one-off decodes, not the polled values
  decoder.quiet = true;
  for (int i = 0; i < CATALOG_ENTRY_COUNT; i++)
  {
    const CatalogEntry &e = CATALOG_ENTRIES[i];
    if (!catalogInModel(e, model) || !catalogMixedLabel(catalogLabel(e)))
      continue;
    const SurveyReg *r = survey.find(e.reg);
    if (r == nullptr || !r->answered || e.offset + e.size > r->len)
      continue;
    LabelDef value(e.reg, e.offset, e.conv, e.size, e.type, catalogLabel(e));
    memset(value.asString, 0, sizeof(value.asString));
    decoder.convert(&value, const_cast<uint8_t *>(r->payload + e.offset));
    if (atof(value.asString) != 0)
      return true;
  }
  return false;
}

void initLabels(){
  //The values of the configured model, from the catalog
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
  catalogTwoZones() = surveyTwoZones(model);
  int refrigerant = catalogBuildLabels(model, config.labels, config.labelCount, catalogLabels, fixes, fixCount, catalogLanguage(config.lang));
  converter.RType = refrigerant ? refrigerant : 802;
  converter.setLabels(catalogLabels.data(), catalogLabels.size());
  mqttSerial.printf("Model: %s (%s), %d values\n", config.model, config.modelConfirmed ? "confirmed" : "to be confirmed", (int)catalogLabels.size());
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
    mqttSerial.printf("No values selected yet.\n");
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
  watchdogHp();
  memset(buffer, 0, SURVEY_BUFFER_SIZE);
  bool ok = queryRegistry(regID, buffer, protocol);
  // Registry 0x40 of an outdoor unit lists its indoor units' addresses: the first one is the one 0x50 is read with
  if (ok && protocol == 'I' && regID == 0x40 && buffer[1] == 0x40 && buffer[2] >= 3)
    x10aIndoorAddress = buffer[3];
  return ok;
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

// Uses the detected model, unless a model is already set for this heat pump.
void applyDetection()
{
  if (detection.model < 0)
  {
    mqttSerial.println("No heat pump answered, check the connection to X10A.");
    return;
  }
  char key[sizeof(config.detectedKey)];
  surveyKey(survey, key, sizeof(key));
  bool sameHeatPump = surveyKeySameUnit(key, config.detectedKey);
  bool forced = forceDetection;
  forceDetection = false;
  if (config.model[0] && sameHeatPump && !forced)
  {
    // Completes a key saved while the outdoor unit slept, the selection stays
    if (strcmp(key, config.detectedKey) != 0 && surveyKeyComplete(key))
    {
      strlcpy(config.detectedKey, key, sizeof(config.detectedKey));
      configSave();
    }
    return;
  }
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

// The hydro board's software, with the 'L' command of the X10A port: 4 bytes, the version's digits reversed
// (04 29 00 18 is 18002904). Boards without the command answer NAK or nothing: the report then has no "iu_sw_id".
static void surveyReadSoftwareId(Survey &s)
{
  uint8_t frame[4], reply[8];
  size_t len = x10aFrame(0x4C, nullptr, 0, frame);
  size_t got = x10aExchange(frame, len, reply, sizeof(reply), 300);
  uint8_t sum = 0;
  for (size_t i = 0; i < 5 && i < got; i++)
    sum += reply[i];
  s.hasSwId = got == 6 && reply[0] == 0x4C && (uint8_t)~sum == reply[5];
  if (s.hasSwId)
    memcpy(s.swId, reply + 1, 4);
}

// The unit's model name at the start of the hydro board's settings memory (the 'I' command, one byte each), where
// the board's software keeps it there: the 2020 softwares do (EHBX08EF6V on 20002203), the 2017-2018 ones read
// zeros (an EAVH16 on 18002904). Boards without the command answer NAK or nothing: no name then.
static void surveyReadUnitModel(Survey &s)
{
  s.unitModel[0] = 0;
  char name[UNIT_MODEL_MAX + 1] = {0};
  for (uint16_t i = 0; i < UNIT_MODEL_MAX; i++)
  {
    uint8_t data[2] = {(uint8_t)i, (uint8_t)(i >> 8)}, frame[8], reply[8];
    size_t len = x10aFrame(0x49, data, 2, frame);
    size_t got = x10aExchange(frame, len, reply, sizeof(reply), 300);
    if (got != 3 || reply[0] != 0x49 || (uint8_t)~(uint8_t)(reply[0] + reply[1]) != reply[2])
      return;
    if (reply[1] == 0)
      break;
    name[i] = (char)reply[1];
  }
  if (unitModelLooksValid(name))
    strlcpy(s.unitModel, name, sizeof(s.unitModel));
}

// Reads every known registry once and builds the detection report. Runs where the registries are polled.
void runSurvey()
{
  surveyRequested = false;
  lastSurvey = millis();
  mqttSerial.println("Starting heat pump survey...");
  surveyInProgress = true;
  surveyRun(survey, surveyQuery);
  if (survey.protocol == 'I')
  {
    surveyReadSoftwareId(survey);
    surveyReadUnitModel(survey);
  }
  surveyInProgress = false;
  static char detectJson[1024];
  detectModel(survey, detection);
  bool reportOk = detectToJson(detection, detectJson, sizeof(detectJson)) != 0;
  //Rendered aside then swapped in: the web interface may be reading the previous report. On the heap for that
  //time (3 KB kept for good as a static); without the room, rendered in place under the lock.
  std::unique_ptr<char[]> report(new (std::nothrow) char[sizeof(surveyJson)]);
  if (report && reportOk)
    reportOk = surveyToJson(survey, report.get(), sizeof(surveyJson), ESPALTHERMA_VERSION, BOARD_NAME, detectJson) != 0;
  valuesLock();
  if (report && reportOk)
    memcpy(surveyJson, report.get(), sizeof(surveyJson));
  else if (!report && reportOk)
    reportOk = surveyToJson(survey, surveyJson, sizeof(surveyJson), ESPALTHERMA_VERSION, BOARD_NAME, detectJson) != 0;
  surveyReportFailed = !reportOk;
  if (!reportOk) surveyJson[0] = 0;
  valuesUnlock();
  report.reset();
  if (!reportOk)
  {
    mqttSerial.println("Survey report too large; partial JSON discarded.");
    eventAddf("Survey report too large; partial JSON discarded");
  }
  mqttSerial.printf("Survey done: protocol %c, %d registries.\n", survey.protocol ? survey.protocol : '-', survey.count);
  applyDetection();
  if (config.model[0] && survey.protocol == 'I')
    layoutFeedSurvey();
  if (config.model[0] && surveyTwoZones(catalogFindModel(config.model)) != catalogTwoZones())
    reloadLabels(); // two zones found (or gone): their values join the recommended ones
  surveyReady = true;
}

void publishSurvey()
{
  surveyReady = false;
  Serial.println(surveyJson);
  mqttPublish(mqttTopics.detect, surveyJson[0] ? surveyJson : "{\"error\":\"survey report too large\"}", true);
}

void hpDelay(unsigned long ms);

// Queries all registries and converts their values. Heat pump task only.
void pollRegistries()
{
  layoutCycle.clear();
  for (size_t i = 0; (i < 32) && (uint8_t)registryIDs[i] != 0xFF; i++)
  {
    watchdogHp();
    diii.hold(1500, diiiSleep); // a registry query must not land in a DIII E0 window
    unsigned char buff[REPLY_BUFFER_SIZE] = {0};
    registryOk[i] = registryPoll((uint8_t)registryIDs[i], buff, sizeof(buff), config.protocol,
      [&]() { return queryRegistry(registryIDs[i], buff, config.protocol); },
      []() { mqttSerial.println("Retrying..."); hpDelay(1000); });
    if (registryOk[i])
    {
      valuesLock();
      converter.readRegistryValues(buff, config.protocol); //process all values from the register
      if (config.protocol == 'I')
        replyCache.update(buff[1], buff + 3, buff[2] >= 2 ? buff[2] - 2 : 0);
      else
        replyCache.update(buff[0], buff + 1, get_reply_len(buff[0], 'S') - 2);
      valuesUnlock();
      if (config.protocol == 'I' && buff[2] >= 2)
        layoutCycle.add(buff[1], buff + 3, buff[2] - 2);
    }
  }
  layoutCheck();
}

// Sends the values of the last query cycle in mqtt. Main loop only.
// Energy and cycle statistics (stats.h), fed from the labels' current values. Main loop, under the values lock.
RTC_NOINIT_ATTR StatsDay statsDay;
Stats stats;
static const HaSensor STATS_LABELS[] = {
    {"Stats: Heat output (kW)", ESP_SENSOR_POWER_KW},
    {"Stats: Electrical input estimated (kW)", ESP_SENSOR_POWER_KW},
    {"Stats: COP estimated", ESP_SENSOR_RATIO},
    {"Stats: COP today heating", ESP_SENSOR_RATIO},
    {"Stats: COP today hot water", ESP_SENSOR_RATIO},
    {"Stats: Heat today heating (kWh)", ESP_SENSOR_ENERGY_KWH},
    {"Stats: Heat today hot water (kWh)", ESP_SENSOR_ENERGY_KWH},
    {"Stats: Heat today cooling (kWh)", ESP_SENSOR_ENERGY_KWH},
    {"Stats: Electricity today heating (kWh)", ESP_SENSOR_ENERGY_KWH},
    {"Stats: Electricity today hot water (kWh)", ESP_SENSOR_ENERGY_KWH},
    {"Stats: Electricity today cooling (kWh)", ESP_SENSOR_ENERGY_KWH},
    {"Stats: Compressor starts today", ESP_SENSOR_COUNT},
    {"Stats: Compressor starts last hour", ESP_SENSOR_COUNT},
    {"Stats: Defrosts today", ESP_SENSOR_COUNT},
    {"Stats: Compressor run today (min)", ESP_SENSOR_MINUTES},
    {"Stats: Backup heater today (min)", ESP_SENSOR_MINUTES},
    {"Stats: Last run (min)", ESP_SENSOR_MINUTES},
    {"Stats: Last pause (min)", ESP_SENSOR_MINUTES},
};

// The DIII values (diii_comm.h appendJson keys): dataType 1 = temperature, conv 105 + "(A)" = current
static const HaSensor DIII_LABELS[] = {
    {"E0: Leaving water (C)", 1},
    {"E0: Return water (C)", 1},
    {"E0: Remote room temp (C)", 1},
    {"E0: DHW tank (C)", 1},
    {"E0: 3-way valve DHW", -1},
    {"E0: Pump", -1},
    {"E0: EVL (pls)", -1},
    {"E0: EVH (pls)", -1},
    {"E0: INV current (A)", -1, 105},
    {"E0: INV speed (rps)", -1},
    {"E0: INV fin temp (C)", 1},
    {"E0: Sync", -1},
    {"63: Leaving water (C)", 1},
    {"63: Return water (C)", 1},
    {"63: R410A liquid (C)", 1},
    {"63: BRC1 room temp (C)", 1},
    {"RAM: Outdoor input current (A)", -1, 105},
    {"RAM: Outdoor rated current (A)", -1, 105},
    {"RAM: Outdoor load (%)", -1},
};

// The current value of the first selected label whose name starts with one of the prefixes (NAN: none, or not a number)
static float statsValue(std::initializer_list<const char *> prefixes, bool *found = nullptr)
{
  for (const char *p : prefixes)
  {
    for (size_t i = 0; i < converter.activeCount; i++)
    {
      const LabelDef &l = converter.activeLabels[i];
      if (strncasecmp(l.label, p, strlen(p)) != 0)
        continue;
      if (found)
        *found = true;
      char *end;
      float v = strtof(l.asString, &end);
      return end == l.asString ? NAN : v;
    }
  }
  return NAN;
}

// ON/OFF labels: 1, 0, or -1 when not selected
static int statsFlag(std::initializer_list<const char *> prefixes)
{
  for (const char *p : prefixes)
  {
    for (size_t i = 0; i < converter.activeCount; i++)
    {
      const LabelDef &l = converter.activeLabels[i];
      if (strncasecmp(l.label, p, strlen(p)) == 0)
        return strcasecmp(l.asString, "ON") == 0 || strcmp(l.asString, "1") == 0 ? 1 : 0;
    }
  }
  return -1;
}

static uint32_t statsEpoch()
{
  time_t t = time(nullptr);
  return t > 1600000000 ? (uint32_t)t : 0;
}

float statsVoltageRead() { return statsValue({"Voltage (N-phase)"}); }

void haExtraSetup();

// A heat sample above the unit's reach is a sensor glitch: 1.6 x the nominal capacity of the definition's series (a
// 16 kW unit makes 15 kW water-side at the start of a run; the old fixed 14 kW cap dropped most of its samples).
// Follows the model: set at start, and again when a detection or the owner changes it.
static float statsHeatCap()
{
  static char forModel[sizeof(config.model)] = "\x01";
  static float cap = STATS_MAX_HEAT_KW;
  if (strcmp(forModel, config.model) != 0)
  {
    strlcpy(forModel, config.model, sizeof(forModel));
    int model = config.model[0] ? catalogFindModel(config.model) : -1;
    cap = model >= 0 && CATALOG_MODELS[model].capMax > 0 ? 1.6f * CATALOG_MODELS[model].capMax : STATS_MAX_HEAT_KW;
  }
  return cap;
}

void statsSetup()
{
  stats.begin(&statsDay);
  stats.settings.voltage = config.statsVoltage;
  stats.settings.phases = config.statsPhases;
  stats.settings.auxW = config.statsAuxW;
  stats.settings.powerFactor = config.statsPowerFactor;
  stats.settings.buhKW[0] = config.statsBuhKW[0];
  stats.settings.buhKW[1] = config.statsBuhKW[1];
  stats.settings.tzMinutes = config.statsTz;
  stats.settings.maxHeatKW = statsHeatCap();
  haExtraSetup();
}

// The sensor type of one of the unit's counters: hours, starts and energy are lifetime totals
static int unitCounterSensorType(const UnitCounterDef &d)
{
  if (unitCounterIsEnergy(d.kind))
    return ESP_SENSOR_ENERGY_KWH;
  return d.kind == UNIT_HOURS ? ESP_SENSOR_HOURS : ESP_SENSOR_TOTAL;
}

// The unit's own energy meter (control.h MeterReader): a lifetime count, a total_increasing sensor in Home Assistant
static const HaSensor METER_LABEL = {"Unit meter: Heat produced (kWh)", ESP_SENSOR_ENERGY_KWH};

// The unit's model name (control.h MeterReader::model), a diagnostic text once read
static const HaSensor UNIT_MODEL_LABEL = {"Unit model", ESP_SENSOR_TEXT};

// Room for every extra sensor: the statistics, the DIII values, the unit's meter and what other features add
static constexpr size_t HA_EXTRA_LABELS_MAX = sizeof(STATS_LABELS) / sizeof(STATS_LABELS[0]) +
                                             sizeof(DIII_LABELS) / sizeof(DIII_LABELS[0]) + 2 + UNIT_COUNTER_COUNT;

// The statistics, the DIII values and the unit's meter share the one list of extra Home Assistant sensors. The
// DIII ones only once the reader found its outdoor unit, the meter once read (no empty sensors on the other units).
void haExtraSetup()
{
  static HaSensor extra[HA_EXTRA_LABELS_MAX];
  size_t n = 0;
  valuesLock();
  if (config.statsEnabled)
    for (const HaSensor &l : STATS_LABELS)
      extra[n++] = l;
  if (diii.status == DIII_READY)
    for (const HaSensor &l : DIII_LABELS)
      extra[n++] = l;
  if (unitMeter.status == CONTROL_READY)
    extra[n++] = METER_LABEL;
  if (unitMeter.model[0])
    extra[n++] = UNIT_MODEL_LABEL;
  for (size_t i = 0; unitMeter.status == CONTROL_READY && i < UNIT_COUNTER_COUNT; i++)
    if (unitMeter.counterMapped(i))
      extra[n++] = HaSensor(UNIT_COUNTER_DEFS[i].label, unitCounterSensorType(UNIT_COUNTER_DEFS[i]));
  haExtraLabels = n ? extra : nullptr;
  haExtraCount = n;
  valuesUnlock(); // readers see the complete list
}

// The DIII values as ATTR keys (mqtt.h, next to the statistics)
// and the unit meter are streamed by MqttValuesView below.
static void statsUpdate()
{
  if (!config.statsEnabled)
    return;
  stats.settings.maxHeatKW = statsHeatCap(); // the model may have been detected or changed since the start
  stats.settings.heaterInTank = strstr(config.model, "ECH2O") != nullptr; // every ECH2O definition carries it in its name
  StatsInput in;
  in.inv = statsValue({"INV frequency", "INV Comp. Frequency"});
  in.flow = statsValue({"Flow sensor"});
  in.lw = statsValue({"Leaving water temp. before BUH", "Outlet Water Heat Exch. Temp.", "[HPSU] Tv inflow Temp", "Leaving water temp. (hydrobox)"});
  in.lwAfter = statsValue({"Leaving water temp. after BUH", "Outlet Water BUH Temp.", "[HPSU] Tvbh inflow Temp"});
  in.inlet = statsValue({"Inlet water temp", "[HPSU] Tr return Temp"});
  in.amp = statsValue({"INV primary current"});
  in.volt = statsValue({"Voltage (N-phase)", "Voltage (V)"}); // the Hybrid names it without the phase
  in.outdoor = statsValue({"R1T-Outdoor air temp", "Outdoor air temp"});
  in.v3 = statsFlag({"3way valve", "Priority to domestic water"});
  in.defrost = statsFlag({"Defrost Operation"});
  in.buh = statsFlag({"BUH Step1"});
  in.buh2 = statsFlag({"BUH Step2"});
  for (size_t i = 0; i < converter.activeCount; i++)
    if (strcasecmp(converter.activeLabels[i].label, "Operation Mode") == 0)
      in.cooling = strcasestr(converter.activeLabels[i].asString, "cool") != nullptr;
  stats.update(in, statsEpoch(), millis() / 1000);
}

// Volatile inputs are sampled once, before counting the MQTT payload. The selected labels and DIII
// readings stay under valuesMutex until the write ends. These small snapshots replace jsonbuff[7120].
struct MqttValues
{
  bool table, oneTopic, binary, withStats, wifi, meterReady, meterKnown;
  uint8_t registries[32];
  size_t registryCount = 0;
  unsigned long at, uptime, freeMem, heatKWh;
  int rssi;
  char ip[16], model[UNIT_MODEL_MAX + 1];
  float statsValues[sizeof(STATS_LABELS) / sizeof(STATS_LABELS[0])];
  uint32_t counters[UNIT_COUNTER_COUNT];
  bool counterMapped[UNIT_COUNTER_COUNT], counterKnown[UNIT_COUNTER_COUNT];
  DiiiComm::JsonOptions diiiOptions;
#ifdef ARDUINO_M5Stick_C_Plus2
  float battery;
#elif defined(ARDUINO_M5Stick_C)
  float vin, ampIn, battery, batteryCurrent, batteryPower;
#endif

  MqttValues()
  {
    table = config.jsonTable;
    oneTopic = config.oneValOneTopic;
    binary = config.binaryAsNumber;
    withStats = config.statsEnabled;
    at = diii.now();
    diiiOptions = diii.jsonOptions();
    uptime = millis() / 1000;
    freeMem = ESP.getFreeHeap();
    wifi = WiFi.status() == WL_CONNECTED;
    rssi = wifi ? WiFi.RSSI() : 0;
    IPAddress address = netIP();
    snprintf(ip, sizeof(ip), "%u.%u.%u.%u", address[0], address[1], address[2], address[3]);
    for (size_t i = 0; i < 32 && (uint8_t)registryIDs[i] != 0xFF; i++)
      if (registryOk[i])
        registries[registryCount++] = (uint8_t)registryIDs[i];

    if (withStats)
    {
      const StatsDay &d = *stats.day;
      float values[] = {stats.heatKW, stats.elecKW, stats.cop, stats.copToday(STATS_HEATING), stats.copToday(STATS_DHW),
                        d.heatKWh[0], d.heatKWh[1], d.heatKWh[2], d.elecKWh[0], d.elecKWh[1], d.elecKWh[2],
                        (float)d.starts, (float)stats.startsLastHour(statsEpoch(), uptime), (float)d.defrosts,
                        d.runSec / 60.0f, d.buhSec / 60.0f, d.lastRunSec / 60.0f, d.lastPauseSec / 60.0f};
      static_assert(sizeof(values) == sizeof(statsValues), "one value per statistics sensor");
      memcpy(statsValues, values, sizeof(values));
    }
    strlcpy(model, unitMeter.model, sizeof(model));
    meterReady = unitMeter.status == CONTROL_READY;
    meterKnown = unitMeter.known;
    heatKWh = unitMeter.heatKWh;
    for (size_t i = 0; i < UNIT_COUNTER_COUNT; i++)
    {
      counterMapped[i] = meterReady && unitMeter.counterMapped(i);
      counterKnown[i] = unitMeter.counterKnown[i];
      counters[i] = unitMeter.counter[i];
    }
#ifdef ARDUINO_M5Stick_C_Plus2
    battery = (float)M5.Power.getBatteryVoltage() / 1000;
#elif defined(ARDUINO_M5Stick_C)
    float acin = M5.Power.Axp192.getACINVoltage(), vbus = M5.Power.Axp192.getVBUSVoltage();
    bool pin = acin > vbus;
    vin = pin ? acin : vbus;
    ampIn = pin ? M5.Power.Axp192.getACINCurrent() : M5.Power.Axp192.getVBUSCurrent();
    battery = M5.Power.Axp192.getBatteryVoltage();
    batteryCurrent = M5.Power.Axp192.getBatteryChargeCurrent() - M5.Power.Axp192.getBatteryDischargeCurrent();
    batteryPower = M5.Power.Axp192.getBatteryPower();
#endif
  }

  template <typename Visit>
  void selected(Visit visit) const
  {
    mqttVisitSelected(converter.activeLabels, converter.activeCount, registries, registryCount, visit);
  }

  void render(MqttJson &json) const
  {
    json.begin(table);
    if (!oneTopic)
      selected([&](const LabelDef &l) { json.converted(l.label, l.asString, binary); });
#ifdef ARDUINO_M5Stick_C_Plus2
    char value[48];
    snprintf(value, sizeof(value), "%.3gV", battery);
    json.text("M5BatV", value);
#elif defined(ARDUINO_M5Stick_C)
    char value[48];
    snprintf(value, sizeof(value), "%.3gV", vin);
    json.text("M5VIN", value);
    snprintf(value, sizeof(value), "%gmA", ampIn);
    json.text("M5AmpIn", value);
    snprintf(value, sizeof(value), "%.3gV", battery);
    json.text("M5BatV", value);
    snprintf(value, sizeof(value), "%gmA", batteryCurrent);
    json.text("M5BatCur", value);
    snprintf(value, sizeof(value), "%.3gmW", batteryPower);
    json.text("M5BatPwr", value);
#endif
    for (size_t i = 0; withStats && i < sizeof(statsValues) / sizeof(statsValues[0]); i++)
      if (isnan(statsValues[i]))
        json.number(STATS_LABELS[i].label, "null");
      else
        json.format(STATS_LABELS[i].label, "%.*f", i >= 11 && i <= 13 ? 0 : 2, statsValues[i]);
    diii.writeJson(json, at, diiiOptions);
    if (model[0])
      json.text(UNIT_MODEL_LABEL.label, model);
    if (meterReady)
    {
      if (meterKnown)
        json.format(METER_LABEL.label, "%lu", heatKWh);
      else
        json.number(METER_LABEL.label, "null");
      for (size_t i = 0; i < UNIT_COUNTER_COUNT; i++)
      {
        if (!counterMapped[i])
          continue;
        unsigned long v = counters[i];
        if (!counterKnown[i])
          json.number(UNIT_COUNTER_DEFS[i].label, "null");
        else if (unitCounterIsEnergy(UNIT_COUNTER_DEFS[i].kind))
          json.format(UNIT_COUNTER_DEFS[i].label, "%lu.%03lu", v / 1000, v % 1000);
        else
          json.format(UNIT_COUNTER_DEFS[i].label, "%lu", v);
      }
    }
    if (wifi)
    {
      char value[24];
      snprintf(value, sizeof(value), "%ddBm", rssi);
      json.text("WifiRSSI", value);
    }
    json.text("IP", ip);
    char diagnostic[24];
    snprintf(diagnostic, sizeof(diagnostic), "%lu", freeMem);
    json.text("FreeMem", diagnostic);
    snprintf(diagnostic, sizeof(diagnostic), "%lu", uptime);
    json.text("Uptime", diagnostic);
    json.end(table);
  }
};

// Main loop, with valuesMutex held for both passes. Incoming MQTT commands retain their 2048-byte buffer.
void sendValues()
{
  if (mqttWriteFailed || !client.connected())
    return;
  mqttSerial.printf("Sending values in MQTT.\n");
  const MqttValues values;
  if (values.oneTopic)
    values.selected([&](const LabelDef &l) {
      char topic[128];
      snprintf(topic, sizeof(topic), "%s%s", config.oneTopicPrefix, l.label);
      mqttPublish(topic, MqttJson::binaryValue(l.asString, values.binary));
    });
  if (!mqttWriteFailed && !mqttStreamJson(client, mqttTopics.attr, false, [&](MqttJson &json) { values.render(json); }))
  {
    mqttWriteFailed = true;
    mqttNet->stop(); // discard a partial MQTT packet before any later publish can follow it
  }
}

void publishValues()
{
  valuesLock();
  statsUpdate();
  sendValues();
  valuesUnlock();
  webSendValues();
}

void hpDelay(unsigned long ms)
{
  delay(ms);
}

// What the controls hold (the targets kept in RAM only, the power limit) lives in the configuration in RAM. Saving
// that configuration in the board's flash at every change wore it for nothing when an automation moves a target
// every few minutes: the values go to RTC memory at once (it survives a restart or an update of the board), and to
// the flash late and seldom (control.h DeferredSave). After a power cut the flash's copy is the one found.
#define HELD_RTC_MAGIC 0x48454C44u // "HELD"
struct HeldRtc
{
  uint32_t magic;
  uint8_t unsaved; // newer than the configuration in flash
  int16_t held[2];
  char origSw[9];
  uint8_t limitOn;
  float limitValue;
  int16_t limitOrig;
  char limitSw[9];
  uint32_t sum;
};
RTC_NOINIT_ATTR HeldRtc heldRtc;
static DeferredSave heldSave;
static ChangeLog targetLog[2], limitLog;

static uint32_t heldRtcSum()
{
  uint32_t h = 2166136261u;
  const uint8_t *p = (const uint8_t *)&heldRtc;
  for (size_t i = 0; i < offsetof(HeldRtc, sum); i++)
    h = (h ^ p[i]) * 16777619u;
  return h;
}

static void heldRtcStore(bool unsaved)
{
  memset(&heldRtc, 0, sizeof(heldRtc));
  heldRtc.magic = HELD_RTC_MAGIC;
  heldRtc.unsaved = unsaved;
  for (int k = 0; k < 2; k++)
    heldRtc.held[k] = (int16_t)config.ctlHeld[k];
  strlcpy(heldRtc.origSw, config.ctlOrigSw, sizeof(heldRtc.origSw));
  heldRtc.limitOn = config.ctlLimitOn;
  heldRtc.limitValue = config.ctlLimitValue;
  heldRtc.limitOrig = (int16_t)config.ctlLimitOrig;
  strlcpy(heldRtc.limitSw, config.ctlLimitSw, sizeof(heldRtc.limitSw));
  heldRtc.sum = heldRtcSum();
}

// A held value changed (the configuration in RAM has it): RTC memory now, the flash when due
static void heldStateChanged(bool atOnce)
{
  heldRtcStore(true);
  heldSave.touch(millis(), atOnce);
}

// At start, after the configuration is read: what RTC memory holds and the flash had not got yet
void heldRestoreAtBoot()
{
  if (heldRtc.magic != HELD_RTC_MAGIC || heldRtc.sum != heldRtcSum() || !heldRtc.unsaved)
    return;
  for (int k = 0; k < 2; k++)
    config.ctlHeld[k] = heldRtc.held[k];
  strlcpy(config.ctlOrigSw, heldRtc.origSw, sizeof(config.ctlOrigSw));
  config.ctlLimitOn = heldRtc.limitOn;
  config.ctlLimitValue = heldRtc.limitValue;
  config.ctlLimitOrig = heldRtc.limitOrig;
  strlcpy(config.ctlLimitSw, heldRtc.limitSw, sizeof(config.ctlLimitSw));
  heldSave.touch(millis(), false);
}

void heldForget()
{
  heldRtc.magic = 0;
}

static void heldSaveTick()
{
  if (!heldSave.due(millis()))
    return;
  configSave();
  heldRtcStore(false);
}

// One target control: identification, requests, saves. Heat pump task only (it owns the X10A link).
static void controlTick(CurveControl &c, bool enabled, const char *what)
{
  if (!enabled || config.protocol != 'I' || busy)
  {
    if (c.status != CONTROL_OFF)
      c.reset();
    return;
  }
  if (c.exchange == nullptr)
    c.exchange = x10aExchange;
  c.setRamOnly(config.ctlRamOnly, millis()); // the MQTT attributes say it, and the range follows
  ControlStatus before = c.status;
  c.tick(millis());
  if (c.status != before)
    mqttSerial.printf("%s target control: %s (software %s)\n", what, controlStatusName(c.status), c.softwareId);
  int k = c.kind;
  if (before != CONTROL_READY && c.status == CONTROL_READY && c.origA < 0 && config.ctlOrig[k][0] >= 0 &&
      strcmp(config.ctlOrigSw, c.softwareId) == 0)
    c.setOriginal(config.ctlOrig[k][0], config.ctlOrig[k][1]); // the unit's curve, saved before the first write
  bool heldKnown = config.ctlHeld[k] >= 0 && strcmp(config.ctlOrigSw, c.softwareId) == 0;
  if (before != CONTROL_READY && c.status == CONTROL_READY && c.held < 0 && config.ctlRamOnly && heldKnown)
    c.held = config.ctlHeld[k]; // held before ESPAltherma restarted: written again if the unit lost it
  if (c.applied >= 0)
  {
    if (c.appliedLo != c.applied)
    {
      mqttSerial.printf("%s curve restored to %d/%d\n", what, c.applied, c.appliedLo);
      eventAddf("%s curve restored to %d/%d", what, c.applied, c.appliedLo);
    }
    else
    {
      mqttSerial.printf("%s target set to %d\n", what, c.applied);
      if (targetLog[k].changed(c.applied, millis())) // at once when the last one is old, counted otherwise
        eventAddf("%s target set to %d", what, c.applied);
    }
    c.applied = c.appliedLo = -1;
  }
  unsigned uncounted = 0;
  if (targetLog[k].flush(millis(), uncounted))
    eventAddf("%s target at %d after %u more changes", what, (int)targetLog[k].last, uncounted);
  if (c.origChanged)
  {
    c.origChanged = false;
    config.ctlOrig[k][0] = c.origA;
    config.ctlOrig[k][1] = c.origB;
    if (c.origA >= 0)
      strlcpy(config.ctlOrigSw, c.softwareId, sizeof(config.ctlOrigSw));
    configSave();
  }
  if (c.heldChanged)
  {
    c.heldChanged = false;
    config.ctlHeld[k] = c.held;
    if (c.held >= 0)
      strlcpy(config.ctlOrigSw, c.softwareId, sizeof(config.ctlOrigSw));
    heldStateChanged(c.held < 0); // a hold that ends is saved at once: it must not come back after a power cut
  }
}

// The power limit: identification, requests, re-application. Heat pump task only.
static void limitTick()
{
  LimitControl &c = limitControl;
  if (!config.powerLimit || config.protocol != 'I' || busy)
  {
    if (c.status != CONTROL_OFF)
      c.reset();
    return;
  }
  if (c.exchange == nullptr)
    c.exchange = x10aExchange;
  ControlStatus before = c.status;
  c.tick(millis());
  if (c.status != before)
    mqttSerial.printf("Power limit control: %s (software %s)\n", controlStatusName(c.status), c.softwareId);
  if (before != CONTROL_READY && c.status == CONTROL_READY && !c.on && config.ctlLimitOn && strcmp(config.ctlLimitSw, c.softwareId) == 0)
    c.restore(true, config.ctlLimitValue, config.ctlLimitOrig); // the limit wanted before a restart
  const char *unit = c.kind == LIMIT_POWER ? "kW" : "A";
  if (c.applied >= 0)
  {
    mqttSerial.printf("Power limit set to %.1f %s\n", c.applied, unit);
    if (limitLog.changed(c.applied, millis()))
      eventAddf("Power limit set to %.1f %s", c.applied, unit);
    c.applied = -1;
  }
  unsigned uncounted = 0;
  if (limitLog.flush(millis(), uncounted))
    eventAddf("Power limit at %.1f %s after %u more changes", limitLog.last, unit, uncounted);
  if (c.released)
  {
    mqttSerial.println("Power limit switched off");
    eventAddf("Power limit switched off");
    c.released = false;
  }
  if (c.stateChanged)
  {
    c.stateChanged = false;
    bool switched = config.ctlLimitOn != c.on; // on or off is saved at once, a new value when due
    config.ctlLimitOn = c.on;
    config.ctlLimitValue = c.value;
    config.ctlLimitOrig = c.origEnable;
    strlcpy(config.ctlLimitSw, c.softwareId, sizeof(config.ctlLimitSw));
    heldStateChanged(switched);
  }
}

// The unit's own energy meter: identification, then a read a minute. Heat pump task only. Not on the HT outdoor
// unit's controller: no meter there, and the DIII reader owns the quiet time.
static void meterTick()
{
  MeterReader &m = unitMeter;
  if (config.protocol != 'I' || strstr(config.model, "ERRQ-ERSQ outdoor") != nullptr)
  {
    if (m.status != CONTROL_OFF)
    {
      m.reset();
      haDeviceModel = "Altherma";
    }
    return;
  }
  if (busy)
    return;
  if (m.exchange == nullptr)
    m.exchange = x10aExchange;
  ControlStatus before = m.status;
  bool hadModel = m.model[0] != 0;
  m.tick(millis());
  if (!hadModel && m.model[0])
  {
    mqttSerial.printf("Unit model: %s\n", m.model);
    haDeviceModel = m.model; // the Home Assistant device shows it, with its own sensor
    haExtraSetup();
    discoveryDirty = true;
  }
  if (m.status == before)
    return;
  if (m.status == CONTROL_READY)
    mqttSerial.printf("Unit energy meter: %lu kWh produced (software %s)\n", (unsigned long)m.heatKWh, m.softwareId);
  else
    mqttSerial.printf("Unit energy meter: %s (software %s)\n", controlStatusName(m.status), m.softwareId);
  if ((before == CONTROL_READY) != (m.status == CONTROL_READY))
  {
    haExtraSetup(); // its sensor joins (or leaves) the Home Assistant discovery
    discoveryDirty = true;
  }
}

// Diagnostics: a read of the hydro board's memory asked from the web page (R command, 16 bytes a time, nothing
// written). The page asks with memoryRequest() and polls memoryResult(); the heat pump task does the reads.
#define MEMORY_READ_MAX 64
static volatile bool memReqPending = false, memReqDone = false, memReqOk = false;
static volatile uint16_t memReqAddr = 0;
static volatile uint8_t memReqLen = 0;
static char memHex[MEMORY_READ_MAX * 2 + 1];

bool memoryRequest(uint16_t addr, uint8_t len)
{
  if (memReqPending || len == 0 || len > MEMORY_READ_MAX)
    return false;
  memReqAddr = addr;
  memReqLen = len;
  memReqDone = false;
  memReqPending = true;
  return true;
}

void memoryResult(JsonDocument &doc)
{
  doc["busy"] = memReqPending;
  doc["done"] = memReqDone;
  doc["ok"] = memReqOk;
  char a[8];
  snprintf(a, sizeof(a), "0x%04X", memReqAddr);
  doc["addr"] = a;
  doc["len"] = memReqLen;
  doc["hex"] = memReqDone && memReqOk ? memHex : "";
}

static void memoryTick()
{
  if (!memReqPending)
    return;
  bool ok = true;
  size_t pos = 0;
  for (uint16_t off = 0; off < memReqLen && ok; off += 16)
  {
    uint16_t at = memReqAddr + off;
    uint8_t a[2] = {(uint8_t)at, (uint8_t)(at >> 8)}, frame[8], reply[24];
    size_t len = x10aFrame(0x52, a, 2, frame);
    size_t got = x10aExchange(frame, len, reply, sizeof(reply), 300);
    uint8_t sum = 0;
    for (size_t i = 0; i < 17 && i < got; i++)
      sum += reply[i];
    ok = got >= 18 && reply[0] == 0x52 && (uint8_t)~sum == reply[17];
    for (size_t i = 0; ok && i < 16 && off + i < memReqLen; i++)
      pos += snprintf(memHex + pos, sizeof(memHex) - pos, "%02X", reply[1 + i]);
  }
  memHex[ok ? pos : 0] = 0;
  memReqOk = ok;
  memReqDone = true;
  memReqPending = false;
}

// Diagnostics: a dump of a whole range of the hydro board's memory, for the people mapping a software without an
// image (R command, 16 bytes a frame, nothing written). Asked from the web page, read by the heat pump task a few
// frames at a time between its polls, kept until the next dump, served by /api/memory/dump as text lines
// "3000: 00 11 ... FF" (a frame that never answered reads "??"), with one header line naming the unit.
#define MEMORY_DUMP_MAX 16384          // bytes in one dump: the kWh block 0x0800-0x0FFF, or 0x3000-0x5FFF (12 KB)
#define MEMORY_DUMP_FRAMES_PER_TICK 16 // about 0.6 s of the heat pump task per tick
#define MEMORY_DUMP_TRIES 3
#define MEMORY_DUMP_KEEP_MS (30UL * 60UL * 1000UL) // the buffer is freed this long after the dump
#define MEMORY_DUMP_LINE 54                         // "XXXX: " + 16 x "XX " with the last space a newline
static uint8_t *dumpBuf = nullptr, *dumpMiss = nullptr; // the bytes, and one bit per frame that did not answer
static volatile uint16_t dumpFrom = 0, dumpLen = 0, dumpPos = 0, dumpMissing = 0;
static volatile bool dumpActive = false, dumpDone = false;
static uint8_t dumpTries = 0;
static unsigned long dumpDoneAt = 0;
static char dumpHeader[160];

static void memoryDumpFree()
{
  free(dumpBuf);
  free(dumpMiss);
  dumpBuf = dumpMiss = nullptr;
  dumpDone = false;
  dumpLen = 0;
}

bool memoryDumpRequest(uint16_t from, uint16_t to)
{
  if (dumpActive || memReqPending || to < from)
    return false;
  uint32_t len = ((uint32_t)to - from + 16) / 16 * 16; // whole frames
  if (len > MEMORY_DUMP_MAX || (uint32_t)from + len > 0x10000)
    return false;
  memoryDumpFree();
  dumpBuf = (uint8_t *)malloc(len);
  dumpMiss = (uint8_t *)calloc(len / 16 / 8 + 1, 1);
  if (dumpBuf == nullptr || dumpMiss == nullptr)
  {
    memoryDumpFree();
    return false;
  }
  snprintf(dumpHeader, sizeof(dumpHeader), "# ESPAltherma %s, %s, software %s, %s, 0x%04X-0x%04X\n", ESPALTHERMA_VERSION,
           unitMeter.model[0] ? unitMeter.model : (survey.unitModel[0] ? survey.unitModel : "unit ?"),
           unitMeter.softwareId[0] ? unitMeter.softwareId : "?", config.model[0] ? config.model : "no model", from, (unsigned)(from + len - 1));
  dumpFrom = from;
  dumpLen = (uint16_t)len;
  dumpPos = 0;
  dumpMissing = 0;
  dumpTries = 0;
  dumpActive = true;
  return true;
}

void memoryDumpStatus(JsonDocument &doc)
{
  doc["active"] = dumpActive;
  doc["done"] = dumpDone;
  doc["from"] = dumpFrom;
  doc["len"] = dumpLen;
  doc["pos"] = dumpPos;
  doc["missing"] = dumpMissing;
}

bool memoryDumpReady() { return dumpDone && dumpBuf != nullptr; }

// The dump as text, from byte index on, for a chunked response: the header line, then one fixed-length line per frame
size_t memoryDumpText(size_t index, uint8_t *out, size_t maxLen)
{
  if (!memoryDumpReady())
    return 0;
  size_t hdr = strlen(dumpHeader), frames = dumpLen / 16, total = hdr + frames * MEMORY_DUMP_LINE, n = 0;
  while (index < total && n < maxLen)
  {
    if (index < hdr)
    {
      out[n++] = dumpHeader[index++];
      continue;
    }
    size_t f = (index - hdr) / MEMORY_DUMP_LINE, o = (index - hdr) % MEMORY_DUMP_LINE;
    char line[MEMORY_DUMP_LINE + 1];
    bool miss = dumpMiss[f / 8] >> (f % 8) & 1;
    int p = snprintf(line, sizeof(line), "%04X: ", (unsigned)(dumpFrom + f * 16));
    for (int i = 0; i < 16; i++)
      p += miss ? snprintf(line + p, sizeof(line) - p, "?? ") : snprintf(line + p, sizeof(line) - p, "%02X ", dumpBuf[f * 16 + i]);
    line[MEMORY_DUMP_LINE - 1] = '\n';
    while (o < MEMORY_DUMP_LINE && n < maxLen)
    {
      out[n++] = line[o++];
      index++;
    }
  }
  return n;
}

static void memoryDumpTick()
{
  if (dumpBuf != nullptr && dumpDone && !dumpActive && millis() - dumpDoneAt > MEMORY_DUMP_KEEP_MS)
    memoryDumpFree(); // the heap back, half an hour after the download was possible
  if (!dumpActive || dumpBuf == nullptr)
    return;
  for (int n = 0; n < MEMORY_DUMP_FRAMES_PER_TICK && dumpPos < dumpLen; n++)
  {
    uint16_t at = dumpFrom + dumpPos;
    uint8_t a[2] = {(uint8_t)at, (uint8_t)(at >> 8)}, frame[8], reply[24];
    size_t len = x10aFrame(0x52, a, 2, frame);
    size_t got = x10aExchange(frame, len, reply, sizeof(reply), 300);
    uint8_t sum = 0;
    for (size_t i = 0; i < 17 && i < got; i++)
      sum += reply[i];
    bool ok = got >= 18 && reply[0] == 0x52 && (uint8_t)~sum == reply[17];
    if (ok)
      memcpy(dumpBuf + dumpPos, reply + 1, 16);
    else if (++dumpTries < MEMORY_DUMP_TRIES)
      return; // the same frame again at the next tick, the poll in between
    else
    {
      memset(dumpBuf + dumpPos, 0, 16);
      dumpMiss[(dumpPos / 16) / 8] |= 1 << ((dumpPos / 16) % 8);
      dumpMissing++;
    }
    dumpTries = 0;
    dumpPos += 16;
  }
  if (dumpPos >= dumpLen)
  {
    dumpActive = false;
    dumpDone = true;
    dumpDoneAt = millis();
    mqttSerial.printf("Memory dump 0x%04X-0x%04X done, %u frames unanswered\n", dumpFrom, (unsigned)(dumpFrom + dumpLen - 1), dumpMissing);
  }
}

void controlLoop()
{
  controlTick(dhwControl, config.dhwTarget, "Hot water");
  controlTick(lwtControl, config.lwtTarget, "Leaving water");
  limitTick();
  meterTick();
  memoryTick();
  memoryDumpTick();
  heldSaveTick();
  // Only on the HT outdoor unit: the hydrobox ports and the other units are never read by it
  bool diiiOn = config.protocol == 'I' && strstr(config.model, "ERRQ-ERSQ outdoor") != nullptr;
  DiiiStatus before = diii.status;
  if (!diiiOn)
  {
    if (diii.status != DIII_OFF)
      diii.reset();
  }
  else if (!busy)
    diii.tick();
  if ((before == DIII_READY) != (diii.status == DIII_READY))
  {
    haExtraSetup(); // its sensors join (or leave) the Home Assistant discovery
    discoveryDirty = true;
  }
}

// Owns the X10A link: runs the survey on request and queries the registries every config.frequency ms.
void hpTask(void *)
{
  for (;;)
  {
    unsigned long start = millis();
    watchdogHp();
    if (!config.model[0] && millis() - lastSurvey > 60000)
    { //No model yet: the heat pump may not have been connected, try again
      surveyRequested = true;
    }
    if (labelsDirty)
    {
      labelsDirty = false;
      HP_STAGE("reload values");
      reloadLabels();
    }
    if (surveyRequested)
    {
      HP_STAGE("survey");
      runSurvey();
    }
    // While a DIII phase is learnt, a 30 s poll would hit the short hydro frame every minute: shift it by 7 s
    unsigned long wait = config.frequency + (diii.learning() ? 7000 : 0);
    if (!busy && (uint8_t)registryIDs[0] != 0xFF)
    {
      HP_STAGE("query");
      pollRegistries();
      lastPollMs = millis();
      cycleReady = true;
      mqttSerial.printf("Done. Waiting %ld ms...", (long)(wait - (millis() - start)));
    }
    HP_STAGE("wait");
    while (millis() - start < wait && !surveyRequested && !labelsDirty)
    {
      watchdogHp();
      HP_STAGE("hot water target");
      controlLoop();
      HP_STAGE("wait");
      delay(diii.idleMs()); // 100 ms, 10 ms while a DIII frame is being learnt or is due
    }
  }
}

void setupPins()
{
  if (config.thermPin >= 0)
  {
    // Its level was set by readEEPROM() just before (the state kept across restarts): an output starts low, which
    // closed an active-low relay until that state was restored, two seconds later on the boards with a screen
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
  heapMark("start");
  restartCauseLoad();
  eventBegin();
  eventAddf("Boot #%u: %s, firmware %s", eventBoot, resetReasonName(), ESPALTHERMA_VERSION);
  Serial.begin(115200);
#if ARDUINO_USB_CDC_ON_BOOT && ARDUINO_USB_MODE
  Serial.setTxTimeoutMs(0); //USB port of the chip: do not block when no computer is connected
#endif
#ifdef SECOND_CONSOLE
  SECOND_CONSOLE.begin(115200);
#endif
  heapMark("serial");
  configLoad();
  heldRestoreAtBoot();
  EEPROM.begin(10);
  readEEPROM(); //the thermostat relay's kept state, as a level, before its pin becomes an output
  setupPins(); //relays to their inactive state first: they would float during the splash screen
  statsSetup();
  heapMark("config, pins, statistics");
  setupScreen();
  heapMark("screen");
  MySerial.begin(9600, SERIAL_CONFIG, config.rxPin, config.txPin);
  heapMark("heat pump serial port");
#ifdef ARDUINO_M5Stick_C_Plus
  gpio_pulldown_dis(GPIO_NUM_25);
  gpio_pullup_dis(GPIO_NUM_25);
#endif

  ArduinoOTA.onStart([]() {
    busy = true;
    watchdogPaused = true; // the upload runs inside ArduinoOTA.handle()
  });

  ArduinoOTA.onError([](ota_error_t error) {
    mqttSerial.print("Error on OTA - restarting");
    restart_board("ArduinoOTA upload failed");
  });

  setupMqttClient(); // also points the log stream at <base>/log
  client.setCallback(callback);
  heapMark("MQTT client");

  //Nothing blocks: WiFi, the setup access point and MQTT are handled by netLoop()
  netBegin();
  heapMark("network started");
  webBegin();
  heapMark("web server");

  initLabels();
  initRegistries();
  requestSurvey();
  heapMark("values list");
  lwtControl.kind = CONTROL_LWT;
  diii.exchange = x10aExchange;
  diii.clock = millis;
  diii.log = diiiLog;
  diii.lock = valuesLock;
  diii.unlock = valuesUnlock;
  xTaskCreate(hpTask, "heatpump", 6144, nullptr, 1, nullptr); // it uses 2.5 KB (/api/heap): the rest is heap the HTTPS connections need
  watchdogBegin();
  heapMark("heat pump task");
  mqttSerial.print("ESPAltherma started!");
  mqttSerial.printf(" Last reset: %s%s%s\n", resetReasonName(), restartCause[0] ? ": " : "", restartCause);
}

void loop()
{
  watchdogLoop();
  LOOP_STAGE("network");
  netLoop();
  LOOP_STAGE("Improv");
  improv.loop();
#ifdef SECOND_CONSOLE
  improv2.loop();
#endif
  LOOP_STAGE("web");
  webLoop();
  //The heat pump task does the querying, we publish its results
  extraLoop();
  if (cycleReady)
  {
    cycleReady = false;
    LOOP_STAGE("publish values");
    publishValues();
    heapMark("first values published");
  }
  if (surveyReady && client.connected())
  {
    LOOP_STAGE("publish survey");
    publishSurvey();
    heapMark("survey published");
  }
  if (discoveryDirty && client.connected())
  {
    discoveryDirty = false;
    LOOP_STAGE("HA discovery");
    publishHomeAssistantDeviceDiscovery();
    heapMark("Home Assistant discovery");
  }
  if (fwUpdate.dirty && client.connected())
  {
    fwUpdate.dirty = false;
    LOOP_STAGE("update entity");
    publishUpdateEntity();
  }
  if ((dhwControl.changed || lwtControl.changed || limitControl.changed) && client.connected())
  {
    LOOP_STAGE("target controls state");
    if (dhwControl.changed)
    {
      dhwControl.changed = false;
      publishControl(dhwControl);
    }
    if (lwtControl.changed)
    {
      lwtControl.changed = false;
      publishControl(lwtControl);
    }
    if (limitControl.changed)
    {
      limitControl.changed = false;
      publishLimit();
    }
  }
  LOOP_STAGE("idle");
  delay(5);
}
