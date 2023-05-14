#include "main.hpp"

bool doRestartInStandaloneWifi = false;

uint16_t loopcount = 0;

#ifdef ARDUINO_M5Stick_C
long LCDTimeout = 40000; // Keep screen ON for 40s then turn off. ButtonA will turn it On again.
#endif

void extraLoop()
{
  while(webOTAIsBusy) {}

  client.loop();

  if(config->CAN_ENABLED)
    canBus_loop();

  if(config->X10A_ENABLED)
    x10a_loop();

#ifdef ARDUINO_M5Stick_C
  if (M5.BtnA.wasPressed()) { // turn back ON screen
    M5.Axp.ScreenBreath(12);
    LCDTimeout = millis() + 30000;
  } else if (LCDTimeout < millis()) { // turn screen off.
    M5.Axp.ScreenBreath(0);
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
#ifdef ARDUINO_M5Stick_C
  M5.begin();
  M5.Axp.EnableCoulombcounter();
  M5.Lcd.setRotation(1);
  M5.Axp.ScreenBreath(12);
  M5.Lcd.fillScreen(TFT_WHITE);
  M5.Lcd.setFreeFont(&FreeSansBold12pt7b);
  m5.Lcd.setTextDatum(MC_DATUM);
  int xpos = M5.Lcd.width() / 2; // half the screen width
  int ypos = M5.Lcd.height() / 2; // half the screen width
  M5.Lcd.setTextColor(TFT_DARKGREY);
  M5.Lcd.drawString("ESPAltherma", xpos, ypos, 1);
  delay(2000);
  M5.Lcd.fillScreen(TFT_BLACK);
  M5.Lcd.setTextFont(1);
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

  if(!LittleFS.begin(true)) {
      Serial.println("An Error has occurred while mounting LittleFS");
      return;
  }

  esp_chip_info_t chip;
  esp_chip_info(&chip);

  debugSerial.printf("ESP32 Model: %i\n", chip.model);
  debugSerial.printf("ESP32 Revision: %i\n", chip.revision);
  debugSerial.printf("ESP32 Cores: %i\n", chip.cores);

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
    x10a_init(config->X10A_CONFIG);
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
    canBus_setup(config->CAN_CONFIG);
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
  reconnectMqtt();
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

    if (!client.connected()) { // (re)connect to MQTT if needed
      reconnectMqtt();
    }

    //debugSerial.printf("Done. Waiting %.2f sec...\n", (float)loopEnd / 1000);
    //waitLoop(loopEnd);
    waitLoop(1000);
  }

  if(mainLoopStatus == LoopRunStatus::Stopping)
    mainLoopStatus = LoopRunStatus::Stopped;
}
