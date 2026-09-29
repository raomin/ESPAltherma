#if defined(ARDUINO_M5Stick_C_Plus2) || defined(ARDUINO_M5Stick_C_Plus) || defined(ARDUINO_M5Stick_C) || defined(ARDUINO_M5Stack_Tough)
#include <M5Unified.h>
#else
#include <Arduino.h>
#endif

#include <WiFi.h>
#include <HardwareSerial.h>

#include <PubSubClient.h>
#include <ArduinoOTA.h>

// No compile time settings: everything is configured at runtime (web interface, stored in NVS).
#include "labeldef.h"

#include "board.h"
#include "version.h"
#include "config.h"
#include "mqttserial.h"
#include "converters.h"
#include "comm.h"
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
bool busy = false;

Survey survey;
char surveyJson[3072];
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
      mqttPublish(topicBuff, labels[i]->asString);
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

// Reads every known registry once and builds the detection report. Runs where the registries are polled.
void runSurvey()
{
  surveyRequested = false;
  lastSurvey = millis();
  mqttSerial.println("Starting heat pump survey...");
  surveyInProgress = true;
  surveyRun(survey, surveyQuery);
  surveyInProgress = false;
  static char detectJson[1024];
  detectModel(survey, detection);
  detectToJson(detection, detectJson, sizeof(detectJson));
  //Rendered aside then swapped in: the web interface may be reading the previous report
  static char report[sizeof(surveyJson)];
  surveyToJson(survey, report, sizeof(report), ESPALTHERMA_VERSION, BOARD_NAME, detectJson);
  valuesLock();
  memcpy(surveyJson, report, sizeof(surveyJson));
  valuesUnlock();
  mqttSerial.printf("Survey done: protocol %c, %d registries.\n", survey.protocol ? survey.protocol : '-', survey.count);
  applyDetection();
  if (config.model[0] && survey.protocol == 'I')
    layoutFeedSurvey();
  surveyReady = true;
}

void publishSurvey()
{
  surveyReady = false;
  Serial.println(surveyJson);
  mqttPublish(MQTT_detect, surveyJson, true);
}

void hpDelay(unsigned long ms);

// Queries all registries and converts their values. Heat pump task only.
void pollRegistries()
{
  layoutCycle.clear();
  for (size_t i = 0; (i < 32) && (uint8_t)registryIDs[i] != 0xFF; i++)
  {
    watchdogHp();
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
  webSendValues();
}

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
    if (!busy && (uint8_t)registryIDs[0] != 0xFF)
    {
      HP_STAGE("query");
      pollRegistries();
      lastPollMs = millis();
      cycleReady = true;
      mqttSerial.printf("Done. Waiting %ld ms...", (long)(config.frequency - (millis() - start)));
    }
    HP_STAGE("wait");
    while (millis() - start < config.frequency && !surveyRequested && !labelsDirty)
    {
      watchdogHp();
      delay(100);
    }
  }
}

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
    watchdogPaused = true; // the upload runs inside ArduinoOTA.handle()
  });

  ArduinoOTA.onError([](ota_error_t error) {
    mqttSerial.print("Error on OTA - restarting");
    restart_board("ArduinoOTA upload failed");
  });

  setupMqttClient();
  client.setCallback(callback);
  mqttSerial.begin(&client, "espaltherma/log");

  //Nothing blocks: WiFi, the setup access point and MQTT are handled by netLoop()
  netBegin();
  webBegin();

  initLabels();
  initRegistries();
  requestSurvey();
  xTaskCreate(hpTask, "heatpump", 8192, nullptr, 1, nullptr);
  watchdogBegin();
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
  }
  if (surveyReady && client.connected())
  {
    LOOP_STAGE("publish survey");
    publishSurvey();
  }
  if (discoveryDirty && client.connected())
  {
    discoveryDirty = false;
    LOOP_STAGE("HA discovery");
    publishHomeAssistantDeviceDiscovery();
  }
  LOOP_STAGE("idle");
  delay(5);
}
