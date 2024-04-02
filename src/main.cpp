#include "main.hpp"

bool doRestartInStandaloneWifi = false;

uint16_t loopcount = 0;

#if defined(ARDUINO_M5Stick_C) || defined(ARDUINO_M5Stack_Tough)
unsigned long LCDTimeout;
bool LCDTimeoutRunning = true;
#define LCD_TIMEOUT_CHECK (millis() - LCDTimeout) >= 30000
#endif

void extraLoop()
{
  while(webOTAIsBusy) {}

  mqttLoop();

  if(config->CAN_ENABLED)
    canBus_loop();

  if(config->X10A_ENABLED)
    x10a_loop();

#if !defined(ARDUINO_M5Stick_C_Plus2) && defined(ARDUINO_M5Stick_C)
  if (M5.BtnA.wasPressed()) { // turn back ON screen
    LCDTimeoutRunning = true;
    M5.Axp.ScreenBreath(12);
    LCDTimeout = millis();
  } else if (LCDTimeoutRunning && LCD_TIMEOUT_CHECK) { // turn screen off.
    M5.Axp.ScreenBreath(0);
    LCDTimeoutRunning = false;
  }
  M5.update();
#elif defined(ARDUINO_M5Stick_C_Plus2)
  if (M5.BtnA.wasPressed()) { // turn back ON screen
    LCDTimeoutRunning = true;
    M5.Display.wakeup();
    LCDTimeout = millis();
  } else if (LCDTimeoutRunning && LCD_TIMEOUT_CHECK) { // turn screen off.
    M5.Display.sleep();
    LCDTimeoutRunning = false;
  }
  M5.update();
#elif ARDUINO_M5Stack_Tough
  if (M5.Touch.changed) { // Turn back ON screen
    M5.Lcd.setBrightness(127);
    M5.Lcd.wakeup();
    LCDTimeout = millis();
    LCDTimeoutRunning = true;
  } else if (LCDTimeoutRunning && LCD_TIMEOUT_CHECK) { // Turn screen off.
    M5.Lcd.sleep();
    M5.Lcd.setBrightness(0);
    LCDTimeoutRunning = false;
  }
  M5.update();
#endif

  if(!doRestartInStandaloneWifi)
   return;

  debugSerial.println("Restarting in standalone wifi mode");
  config->STANDALONE_WIFI = true;
  saveConfig();
  restart_board();
}

void setupScreen()
{
#if !defined(ARDUINO_M5Stick_C_Plus2) && defined(ARDUINO_M5Stick_C) || defined(ARDUINO_M5Stack_Tough)
  M5.begin();
  M5.Axp.EnableCoulombcounter();
  M5.Lcd.setRotation(1);
  M5.Axp.ScreenBreath(12);
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
  int xpos = M5.Lcd.width() / 2; // half the screen width
  int ypos = M5.Lcd.height() / 2; // half the screen width
  M5.Lcd.setTextColor(TFT_DARKGREY);
  M5.Lcd.drawString("ESPAltherma", xpos, ypos);
  delay(2000);
  M5.Lcd.fillScreen(TFT_BLACK);
  M5.Lcd.setFont(&Font0);
  M5.Lcd.setTextColor(TFT_GREEN);
#endif
}

void IRAM_ATTR restartInStandaloneWifi()
{
  doRestartInStandaloneWifi = true;
}

void setup()
{
  Serial.begin(115200);

  if(!LittleFS.begin(LITTLEFS_BEGIN_PARAM)) {
      Serial.println("An Error has occurred while mounting LittleFS");
      return;
  }

  #ifdef ARDUINO_ARCH_ESP8266
  debugSerial.printf("ESP8266 Chip ID:%i\n", ESP.getFlashChipId());
  debugSerial.printf("ESP8266 Real Size:%i\n", ESP.getFlashChipRealSize());
  debugSerial.printf("ESP8266 Size: %i\n", ESP.getFlashChipSize());
  debugSerial.printf("ESP8266 Speed%i\n", ESP.getFlashChipSpeed());
  debugSerial.printf("ESP8266 Mode%i\n", ESP.getFlashChipMode());
  #else
  esp_chip_info_t chip;
  esp_chip_info(&chip);

  debugSerial.printf("ESP32 Model: %i\n", chip.model);
  debugSerial.printf("ESP32 Revision: %i\n", chip.revision);
  debugSerial.printf("ESP32 Cores: %i\n", chip.cores);
  #endif

  initPersistence();

  readConfig();

  if(config->STANDALONE_WIFI || !config->configStored) {
    debugSerial.println("Start in standalone mode..");
    start_standalone_wifi();
    WebUI_Init();
  }

  initMQTT();

  setupScreen();

  if(!config->configStored) {
    debugSerial.println("No config found, skip setup...");
    return;
  }

  if(config->X10A_ENABLED) {
    X10A::debugStream = &debugSerial;
    callbackX10A_updateValues = updateValues;
    callbackX10A_sendValues = sendValues;
    callbackX10A_wait = waitLoop;
    x10a_init(config->X10A_CONFIG, config->MQTT_DISABLE_LOG_MESSAGES); // TODO: Correct place to use a MQTT config variable inside X10A??
    x10a_initRegistries(&registryBuffers, registryBufferSize);
  }

  if(config->HEATING_ENABLED) {
    pinMode(config->PIN_HEATING, OUTPUT);
    digitalWrite(config->PIN_HEATING, HIGH);
  }

  if(config->COOLING_ENABLED) {
    pinMode(config->PIN_COOLING, OUTPUT);
    digitalWrite(config->PIN_COOLING, HIGH);
  }

  if(config->SG_ENABLED) {
    // Smartgrid pins - Set first to the inactive state, before configuring as outputs (avoid false triggering when initializing)
    digitalWrite(config->PIN_SG1, SG_RELAY_INACTIVE_STATE);
    digitalWrite(config->PIN_SG2, SG_RELAY_INACTIVE_STATE);
    pinMode(config->PIN_SG1, OUTPUT);
    pinMode(config->PIN_SG2, OUTPUT);

    debugSerial.printf("Configured SG Pins %u %u - State: %u\n", config->PIN_SG1, config->PIN_SG2, SG_RELAY_INACTIVE_STATE);
  }

  if(config->CAN_ENABLED) {
    CAN::debugStream = &debugSerial;
    callbackRecievedCommand = [](const char *label, const char *value) { mqttPublish(MQTTPublishTopic::CAN, value, label); };
    callbackCAN = canBus_set;
    canBus_setup(config->CAN_CONFIG);
  }

  if(config->SAFETY_ENABLED) {
    pinMode(config->PIN_SAFETY, OUTPUT);
    digitalWrite(config->PIN_SAFETY, LOW);
  }

#ifdef ARDUINO_M5Stick_C_Plus
  gpio_pulldown_dis(GPIO_NUM_25);
  gpio_pullup_dis(GPIO_NUM_25);
#endif

  readPersistence(); // restore previous state

  if(!config->STANDALONE_WIFI) {
    debugSerial.println("Setting up wifi...");
    setup_wifi();
    WebUI_Init();
  }

  pinMode(config->PIN_ENABLE_CONFIG, INPUT_PULLUP);
  attachInterrupt(config->PIN_ENABLE_CONFIG, restartInStandaloneWifi, FALLING);

  debugSerial.print("Connecting to MQTT server...\n");
  connectMqtt();
  mqttInitCallbacks();
  debugSerial.println("OK!");

  debugSerial.print("ESPAltherma started!\n");
}

void waitLoop(ulong ms)
{
  ulong start = millis();
  while (millis() < start + ms) { // wait .5sec between registries

    if(valueX10ALoadState == Pending ||
       valueCANLoadState == Pending ||
       wifiLoadState  == Pending ||
       mainLoopStatus == LoopRunStatus::Stopping) {
      return;
    }

    extraLoop();
  }
}

void loop()
{
  if(mainLoopStatus == LoopRunStatus::Stopped)
    return;

  if (wifiLoadState == ValueLoadState::NotLoading &&
      !config->STANDALONE_WIFI &&
      config->configStored &&
      WiFi.status() != WL_CONNECTED) {
    //restart board if needed
    checkWifi();
  }

  if(!config->configStored) {
    extraLoop();
  } else {
    webuiScanX10ARegister();
    webuiScanCANRegister();
    webuiScanWifi();

    if (!mqttConnected()) { // (re)connect to MQTT if needed
      reconnectMqtt();
    }

    //debugSerial.printf("Done. Waiting %.2f sec...\n", (float)loopEnd / 1000);
    //waitLoop(loopEnd);
    waitLoop(1000);
  }

  if(mainLoopStatus == LoopRunStatus::Stopping)
    mainLoopStatus = LoopRunStatus::Stopped;
}
