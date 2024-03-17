#ifdef ARDUINO_M5Stick_C_Plus2
#include <M5StickCPlus2.h>
#elif ARDUINO_M5Stick_C_Plus
#include <M5StickCPlus.h>
#elif ARDUINO_M5Stick_C
#include <M5StickC.h>
#elif ARDUINO_M5Stack_Tough
#include <M5Tough.h>
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

#if __has_include("my_setup.h")
#include "my_setup.h"
#else
#include "setup.h"
#endif

#include "mqttserial.h"
#include "converters.h"
#include "comm.h"
#include "mqtt.h"
#include "restart.h"

Converter converter;
char registryIDs[32]; //Holds the registries to query
bool busy = false;

#if defined(ARDUINO_M5Stick_C) || defined(ARDUINO_M5Stick_C_Plus) || defined(ARDUINO_M5Stack_Tough)
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

    #ifdef ONEVAL_ONETOPIC
    char topicBuff[128] = MQTT_OneTopic;
    strcat(topicBuff,labels[i]->label);
    client.publish(topicBuff, labels[i]->asString);

    #else
    if (alpha){      

      snprintf(jsonbuff + strlen(jsonbuff), MAX_MSG_SIZE - strlen(jsonbuff), "\"%s\":\"%s\",", labels[i]->label, labels[i]->asString);
    }
    else{//number, no quotes
      snprintf(jsonbuff + strlen(jsonbuff), MAX_MSG_SIZE - strlen(jsonbuff), "\"%s\":%s,", labels[i]->label, labels[i]->asString);
    }
    #endif
  }
}

uint16_t loopcount =0;
boolean display_sleeping = false;

void extraLoop()
{
  client.loop();
  ArduinoOTA.handle();
  while (busy)
  { //Stop processing during OTA
    ArduinoOTA.handle();
  }

#if !defined(ARDUINO_M5Stick_C_Plus2) && defined(ARDUINO_M5Stick_C) || defined(ARDUINO_M5Stick_C_Plus) 
  if (M5.BtnA.wasPressed()){//Turn back ON screen
    M5.Axp.ScreenBreath(12);
    LCDTimeout = millis() + 30000;
  } else if (LCDTimeout < millis()) { //Turn screen off.
    M5.Axp.ScreenBreath(0);
  }
  M5.update();
#endif


#if defined(ARDUINO_M5Stick_C_Plus2)
  if (M5.BtnA.wasPressed()){//Turn back ON screen
    M5.Display.wakeup();
    LCDTimeout = millis() + 30000;
  } else if (LCDTimeout < millis()) { //Turn screen off.
    M5.Display.sleep();
  }
  M5.update();
#endif

#ifdef ARDUINO_M5Stack_Tough
  if (M5.Touch.changed){//Turn back ON screen
    M5.Lcd.setBrightness(12);
    M5.Lcd.wakeup();
    LCDTimeout = millis() + 30000;
    display_sleeping = false;

  }else if (LCDTimeout < millis() & !display_sleeping){//Turn screen off.
    M5.Lcd.sleep();
    M5.Lcd.setBrightness(0);
    display_sleeping = true;
  }
  M5.update();
#endif
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
  int i = 0;
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
    if (i++ == 240)
    {
      Serial.printf("Tried connecting for 120 sec, rebooting now.");
      restart_board();
    }
  }
}

void setup_wifi()
{
  delay(10);
  // We start by connecting to a WiFi network
  mqttSerial.printf("Connecting to %s\n", WIFI_SSID);

  #if defined(WIFI_IP) && defined(WIFI_GATEWAY) && defined(WIFI_SUBNET)
    IPAddress local_IP(WIFI_IP);
    IPAddress gateway(WIFI_GATEWAY);
    IPAddress subnet(WIFI_SUBNET);

    #ifdef WIFI_PRIMARY_DNS
      IPAddress primaryDNS(WIFI_PRIMARY_DNS);
    #else
      IPAddress primaryDNS();
    #endif

    #ifdef WIFI_SECONDARY_DNS
      IPAddress secondaryDNS(WIFI_SECONDARY_DNS);
    #else
      IPAddress secondaryDNS();
    #endif

    if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
      mqttSerial.println("Failed to set static ip!");
    }
  #endif

  uint8_t *bssid = nullptr;
  uint32_t wifi_channel = 0;
#ifdef ARDUINO_ARCH_ESP8266
  get_wifi_bssid(WIFI_SSID, bssid, &wifi_channel);
#else //assume ESP32
  WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);
  WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
#endif
    
  if (bssid != nullptr)
  {
    WiFi.begin(WIFI_SSID, WIFI_PWD, wifi_channel, bssid);
  }
  else
  {
    WiFi.begin(WIFI_SSID, WIFI_PWD, 0, 0, true);
  }
  checkWifi();
  mqttSerial.printf("Connected. IP Address: %s\n", WiFi.localIP().toString().c_str());
}

void initRegistries(){
    //getting the list of registries to query from the selected values
  for (size_t i = 0; i < sizeof(registryIDs); i++)
  {
    registryIDs[i]=0xff;
  }

  int i = 0;
  for (auto &&label : labelDefs)
  {
    if (!contains(registryIDs, sizeof(registryIDs), label.registryID))
    {
      mqttSerial.printf("Adding registry 0x%2x to be queried.\n", label.registryID);
      registryIDs[i++] = label.registryID;
    }
  }
  if (i == 0)
  {
    mqttSerial.printf("ERROR - No values selected in the include file. Stopping.\n");
    while (true)
    {
      extraLoop();
    }
  }
}

void setupScreen(){
#if !defined(ARDUINO_M5Stick_C_Plus2) && defined(ARDUINO_M5Stick_C) || defined(ARDUINO_M5Stick_C_Plus) || defined(ARDUINO_M5Stack_Tough)
  M5.begin();
#if !defined(ARDUINO_M5Stick_C_Plus2)
  M5.Axp.EnableCoulombcounter();
#endif
  M5.Lcd.setRotation(1);
  M5.Axp.ScreenBreath(127);
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
#if !defined(ARDUINO_M5Stick_C_Plus2)
  M5.Axp.EnableCoulombcounter();
#endif
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

void setup()
{
  Serial.begin(115200);
  setupScreen();
  MySerial.begin(9600, SERIAL_CONFIG, RX_PIN, TX_PIN);
  pinMode(PIN_THERM, OUTPUT);
  // digitalWrite(PIN_THERM, PIN_THERM_ACTIVE_STATE);

#ifdef SAFETY_RELAY_PIN
  pinMode(SAFETY_RELAY_PIN, OUTPUT);
  digitalWrite(SAFETY_RELAY_PIN, !SAFETY_RELAY_ACTIVE_STATE);
#endif

#ifdef PIN_SG1
  //Smartgrid pins - Set first to the inactive state, before configuring as outputs (avoid false triggering when initializing)
  digitalWrite(PIN_SG1, SG_RELAY_INACTIVE_STATE);
  digitalWrite(PIN_SG2, SG_RELAY_INACTIVE_STATE);
  pinMode(PIN_SG1, OUTPUT);
  pinMode(PIN_SG2, OUTPUT);

#endif
#ifdef ARDUINO_M5Stick_C_Plus
  gpio_pulldown_dis(GPIO_NUM_25);
  gpio_pullup_dis(GPIO_NUM_25);
#endif

  EEPROM.begin(10);
  readEEPROM();//Restore previous state
  mqttSerial.print("Setting up wifi...");
  setup_wifi();
  ArduinoOTA.setHostname("ESPAltherma");
  ArduinoOTA.onStart([]() {
    busy = true;
  });

  ArduinoOTA.onError([](ota_error_t error) {
    mqttSerial.print("Error on OTA - restarting");
    restart_board();
  });
  ArduinoOTA.begin();

  #ifdef MQTT_ENCRYPTED
  // Required to establish encrypted connections. 
  // If you want to be more secure here, you can use the CA certificate to allow the wifi client to verify the other party. NOTE: If you use the CA certificate here, then you need to make sure to update it here regulary!
  espClient.setInsecure();
  espClient.setTimeout(5);
  #endif

  client.setBufferSize(MAX_MSG_SIZE); //to support large json message
  client.setCallback(callback);
  client.setServer(MQTT_SERVER, MQTT_PORT);

  auto timeout = espClient.getTimeout();
  Serial.printf("Wifi client timeout: %d\n", timeout);

  mqttSerial.printf("Connecting to MQTT server: %s:%d\n", MQTT_SERVER, MQTT_PORT);
  mqttSerial.begin(&client, "espaltherma/log");
  reconnectMqtt();
  mqttSerial.println("OK!");

  initRegistries();
  mqttSerial.print("ESPAltherma started!");
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
  if (WiFi.status() != WL_CONNECTED)
  { //restart board if needed
    checkWifi();
  }
  if (!client.connected())
  { //(re)connect to MQTT if needed
    reconnectMqtt();
  }
  //Querying all registries
  for (size_t i = 0; (i < 32) && registryIDs[i] != 0xFF; i++)
  {
    unsigned char buff[64] = {0};
    int tries = 0;
    while (!queryRegistry(registryIDs[i], buff, PROTOCOL) && tries++ < 3)
    {
      mqttSerial.println("Retrying...");
      waitLoop(1000);
    }
    unsigned char receivedRegistryID = PROTOCOL == 'S' ? buff[0] : buff[1];
    if (registryIDs[i] == receivedRegistryID) //if replied registerID is coherent with the command
    {
      converter.readRegistryValues(buff, PROTOCOL); //process all values from the register
      updateValues(registryIDs[i]);       //send them in mqtt
      //waitLoop(500);//wait .5sec between registries
    }
  }
  sendValues();//Send the full json message
  mqttSerial.printf("Done. Waiting %ld ms...", FREQUENCY - millis() + start);
  waitLoop(FREQUENCY - millis() + start);
}
