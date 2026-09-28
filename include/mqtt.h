#include <PubSubClient.h>
#include <EEPROM.h>
#include "restart.h"
#include "comm.h"

#define MQTT_attr "espaltherma/ATTR"
#define MQTT_lwt "espaltherma/LWT"
#define MQTT_detect "espaltherma/detect"

#define EEPROM_CHK 1
#define EEPROM_STATE 0

#ifndef MAX_MSG_SIZE
#define MAX_MSG_SIZE 7120//max size of the json message sent in mqtt
#endif

char jsonbuff[MAX_MSG_SIZE] = "{\0";

WiFiClient plainClient;
#if defined(ESPALTHERMA_GENERIC) || defined(MQTT_ENCRYPTED)
// TLS is a runtime option of the generic firmware; builds from my_setup.h opt in with MQTT_ENCRYPTED (~100KB of flash)
#include <WiFiClientSecure.h>
#define HAS_TLS_CLIENT
WiFiClientSecure secureClient;
#endif
PubSubClient client;
Client *mqttNet = &plainClient; // the network client under PubSubClient

extern Converter converter;

// Publishes, and flags a write the broker did not take (stage.h). A message larger than the buffer is not one.
bool mqttPublish(const char *topic, const char *payload, bool retained = false)
{
  if (mqttWriteFailed || !client.connected())
    return false;
  size_t len = strlen(payload);
  if (MQTT_MAX_HEADER_SIZE + 2 + strlen(topic) + len > client.getBufferSize())
    return false;
  if (client.publish(topic, (const uint8_t *)payload, len, retained))
    return true;
  mqttWriteFailed = true;
  return false;
}

// Defined in main.cpp
void requestSurvey();
void valuesLock();
void valuesUnlock();

void resetJson()
{
  strcpy(jsonbuff, config.jsonTable ? "[{" : "{");
}

// Selects the (plain or TLS) network client and the broker, from the configuration.
void setupMqttClient()
{
#ifdef HAS_TLS_CLIENT
  if (config.mqttTls)
  {
    // Required to establish encrypted connections.
    // If you want to be more secure here, you can use the CA certificate to allow the wifi client to verify the other party. NOTE: If you use the CA certificate here, then you need to make sure to update it here regulary!
    secureClient.setInsecure();
    secureClient.setTimeout(5);
    client.setClient(secureClient);
    mqttNet = &secureClient;
    Serial.printf("Wifi client timeout: %d\n", secureClient.getTimeout());
  }
  else
#endif
  {
    client.setClient(plainClient);
    mqttNet = &plainClient;
    Serial.printf("Wifi client timeout: %d\n", plainClient.getTimeout());
  }
  resetJson();
  client.setBufferSize(MAX_MSG_SIZE); //to support large json message
  client.setServer(config.mqttServer, config.mqttPort);
}

// Relay states, for the web interface
bool thermostatOn = false;
bool safetyActive = false; // the heat pump is stopped by the safety relay
int sgMode = 0;

int thermActiveState() { return config.thermActiveHigh ? HIGH : LOW; }
int sgActiveState() { return config.sgActiveHigh ? HIGH : LOW; }
int sgInactiveState() { return config.sgActiveHigh ? LOW : HIGH; }
int safetyActiveState() { return config.safetyActiveHigh ? HIGH : LOW; }

/*
 * Publishes a retained device discovery profile to MQTT, so Home Assistant can auto-configure our device and its sensors.
 */
void publishHomeAssistantDeviceDiscovery();

void sendValues()
{
  mqttSerial.printf("Sending values in MQTT.\n");
#ifdef ARDUINO_M5Stick_C_Plus2
  //Add Power values
  // getBatteryVoltage returns battery voltage [mV] as an int16_t
  float batteryVoltage = (float) M5.Power.getBatteryVoltage() / 1000; // convert to V as a float
  snprintf(jsonbuff + strlen(jsonbuff),MAX_MSG_SIZE - strlen(jsonbuff) , "\"%s\":\"%.3gV\",", "M5BatV", batteryVoltage);
#elif ARDUINO_M5Stick_C
  //Add M5 APX values
  { // the input in use: the 5V pin (ACIN, eg. powered from the X10A) or USB (VBUS)
    float acin = M5.Power.Axp192.getACINVoltage(), vbus = M5.Power.Axp192.getVBUSVoltage();
    bool pin = acin > vbus;
    snprintf(jsonbuff + strlen(jsonbuff),MAX_MSG_SIZE - strlen(jsonbuff) , "\"%s\":\"%.3gV\",\"%s\":\"%gmA\",", "M5VIN", pin ? acin : vbus,"M5AmpIn", pin ? M5.Power.Axp192.getACINCurrent() : M5.Power.Axp192.getVBUSCurrent());
  }
  snprintf(jsonbuff + strlen(jsonbuff),MAX_MSG_SIZE - strlen(jsonbuff) , "\"%s\":\"%.3gV\",\"%s\":\"%gmA\",", "M5BatV", M5.Power.Axp192.getBatteryVoltage(),"M5BatCur", M5.Power.Axp192.getBatteryChargeCurrent() - M5.Power.Axp192.getBatteryDischargeCurrent());
  snprintf(jsonbuff + strlen(jsonbuff),MAX_MSG_SIZE - strlen(jsonbuff) , "\"%s\":\"%.3gmW\",", "M5BatPwr", M5.Power.Axp192.getBatteryPower());
#endif
  if (WiFi.status() == WL_CONNECTED) // not over Ethernet
    snprintf(jsonbuff + strlen(jsonbuff),MAX_MSG_SIZE - strlen(jsonbuff) , "\"%s\":\"%ddBm\",", "WifiRSSI", WiFi.RSSI());
  snprintf(jsonbuff + strlen(jsonbuff),MAX_MSG_SIZE - strlen(jsonbuff) , "\"%s\":\"%d\",", "FreeMem", ESP.getFreeHeap());
  snprintf(jsonbuff + strlen(jsonbuff),MAX_MSG_SIZE - strlen(jsonbuff) , "\"%s\":\"%lu\",", "Uptime", (unsigned long)(millis() / 1000)); // seconds
  jsonbuff[strlen(jsonbuff) - 1] = '}';
  if (config.jsonTable)
  {
    strcat(jsonbuff,"]");
  }
  mqttPublish(MQTT_attr, jsonbuff);
  resetJson();
}

void saveEEPROM(uint8_t state){
    EEPROM.write(EEPROM_STATE,state);
    EEPROM.commit();
}

void readEEPROM(){
  if ('R' == EEPROM.read(EEPROM_CHK)){
    if (config.thermPin >= 0){
      digitalWrite(config.thermPin,EEPROM.read(EEPROM_STATE));
    }
    thermostatOn = EEPROM.read(EEPROM_STATE) == thermActiveState();
    mqttSerial.printf("Restoring previous state: %s",(EEPROM.read(EEPROM_STATE) == thermActiveState())? "On":"Off" );
  }
  else{
    mqttSerial.printf("EEPROM not initialized (%d). Initializing...",EEPROM.read(EEPROM_CHK));
    EEPROM.write(EEPROM_CHK,'R');
    EEPROM.write(EEPROM_STATE,!thermActiveState());
    EEPROM.commit();
    if (config.thermPin >= 0){
      digitalWrite(config.thermPin,!thermActiveState());
    }
  }
}

// Defined in main.cpp; used to keep the screen/button responsive and WiFi
// recovering while we wait between MQTT connection attempts.
void handleScreen();
void checkWifi();

// One connection attempt; on success publishes the discovery messages and subscribes.
bool mqttConnectOnce()
{
    mqttSerial.print("Attempting MQTT connection...");

    if (client.connect(config.mqttClientId, config.mqttUser, config.mqttPwd, MQTT_lwt, 0, true, "Offline"))
    {
      mqttSerial.println("connected!");
      client.publish("homeassistant/sensor/espAltherma/config", "{\"name\":\"AlthermaSensors\",\"stat_t\":\"~/LWT\",\"avty_t\":\"~/LWT\",\"pl_avail\":\"Online\",\"pl_not_avail\":\"Offline\",\"uniq_id\":\"espaltherma\",\"device\":{\"identifiers\":[\"ESPAltherma\"]}, \"~\":\"espaltherma\",\"json_attr_t\":\"~/ATTR\"}", true);
      client.publish(MQTT_lwt, "Online", true);
      if (config.thermPin >= 0)
      {
        client.publish("homeassistant/switch/espAltherma/config", "{\"name\":\"Altherma\",\"cmd_t\":\"~/POWER\",\"stat_t\":\"~/STATE\",\"pl_off\":\"OFF\",\"pl_on\":\"ON\",\"~\":\"espaltherma\"}", true);
      }
      else
      {
        // No thermostat relay: remove the switch from HA
        client.publish("homeassistant/switch/espAltherma/config", "", true);
      }

      publishHomeAssistantDeviceDiscovery();

      // Subscribe
      client.subscribe("espaltherma/POWER");
      client.subscribe("espaltherma/detect/run");
      if (config.sg1Pin >= 0)
      {
      // Smart Grid
      client.publish("homeassistant/select/espAltherma/sg/config", "{\"availability\":[{\"topic\":\"espaltherma/LWT\",\"payload_available\":\"Online\",\"payload_not_available\":\"Offline\"}],\"availability_mode\":\"all\",\"unique_id\":\"espaltherma_sg\",\"device\":{\"identifiers\":[\"ESPAltherma\"],\"manufacturer\":\"ESPAltherma\",\"model\":\"M5StickC PLUS ESP32-PICO\",\"name\":\"ESPAltherma\"},\"icon\":\"mdi:solar-power\",\"name\":\"EspAltherma Smart Grid\",\"command_topic\":\"espaltherma/sg/set\",\"command_template\":\"{% if value == 'Free Running' %} 0 {% elif value == 'Forced Off' %} 1 {% elif value == 'Recommended On' %} 2 {% elif value == 'Forced On' %} 3 {% else %} 0 {% endif %}\",\"options\":[\"Free Running\",\"Forced Off\",\"Recommended On\",\"Forced On\"],\"state_topic\":\"espaltherma/sg/state\",\"value_template\":\"{% set mapper = { '0':'Free Running', '1':'Forced Off', '2':'Recommended On', '3':'Forced On' } %} {% set word = mapper[value] %} {{ word }}\"}", true);
      client.subscribe("espaltherma/sg/set");
      client.publish("espaltherma/sg/state", "0");
      }
      else
      {
      // Publish empty retained message so discovered entities are removed from HA
      client.publish("homeassistant/select/espAltherma/sg/config", "", true);
      }

      if (config.safetyPin >= 0)
      {
      // Safety relay
      client.publish("homeassistant/switch/espAltherma/safety/config", "{\"name\":\"Altherma Safety\",\"cmd_t\":\"~/SAFETY\",\"stat_t\":\"~/SAFETY_STATE\",\"pl_off\":\"0\",\"pl_on\":\"1\",\"~\":\"espaltherma\"}", true);
      client.subscribe("espaltherma/SAFETY");
      }

      if (config.debugSerial)
      {
      // DebugSerial - MQTT<>Serial gateway
      client.subscribe("espaltherma/serialTX");
      }
      return true;
    }
    mqttSerial.printf("failed, rc=%d, try again in 5 seconds", client.state());
    return false;
}

// Blocks until connected (ESP8266; the ESP32 retries from the main loop, see netmgr.h)
void reconnectMqtt()
{
  // Loop until we're reconnected
  int i = 0;
  while (!client.connected())
  {
    if (WiFi.status() != WL_CONNECTED)
    { // No point retrying MQTT without WiFi; recover it first
      checkWifi();
    }
    if (!mqttConnectOnce())
    {
      unsigned long start = millis();
      while (millis() < start + 5000)
      {
        ArduinoOTA.handle();
        handleScreen();//Keep the button responsive while retrying
        mqttSerial.drain();
        delay(10);
      }

      if (i++ == 100) {
        mqttSerial.printf("Tried for 500 sec, rebooting now.");
        restart_board("MQTT broker unreachable for 500 seconds");
      }
    }
  }
}

void callbackTherm(byte *payload, unsigned int length)
{
  payload[length] = '\0';

  // Is it ON or OFF?
  // Ok I'm not super proud of this, but it works :p
  if (config.thermPin < 0 && payload[0] != 'R')
  {
    mqttSerial.println("No thermostat relay configured, ignoring");
  }
  else if (payload[1] == 'F')
  { //turn off
    digitalWrite(config.thermPin, !thermActiveState());
    saveEEPROM(!thermActiveState());
    thermostatOn = false;
    client.publish("espaltherma/STATE", "OFF", true);
    mqttSerial.println("Turned OFF");
  }
  else if (payload[1] == 'N')
  { //turn on
    digitalWrite(config.thermPin, thermActiveState());
    saveEEPROM(thermActiveState());
    thermostatOn = true;
    client.publish("espaltherma/STATE", "ON", true);
    mqttSerial.println("Turned ON");
  }
  else if (payload[0] == 'R')//R(eset/eboot)
  {
    mqttSerial.println("Rebooting");
    delay(100);
    restart_board("Restart requested over MQTT");
  }
  else
  {
    mqttSerial.printf("Unknown message: %s\n", payload);
  }
}

//Smartgrid callbacks
void callbackSg(byte *payload, unsigned int length)
{
  payload[length] = '\0';

  if (payload[0] == '0')
  {
    // Set SG 0 mode => SG1 = INACTIVE, SG2 = INACTIVE
    digitalWrite(config.sg1Pin, sgInactiveState());
    digitalWrite(config.sg2Pin, sgInactiveState());
    sgMode = 0;
    client.publish("espaltherma/sg/state", "0");
    mqttSerial.println("Set SG mode to 0 - Normal operation");
  }
  else if (payload[0] == '1')
  {
    // Set SG 1 mode => SG1 = INACTIVE, SG2 = ACTIVE
    digitalWrite(config.sg1Pin, sgInactiveState());
    digitalWrite(config.sg2Pin, sgActiveState());
    sgMode = 1;
    client.publish("espaltherma/sg/state", "1");
    mqttSerial.println("Set SG mode to 1 - Forced OFF");
  }
  else if (payload[0] == '2')
  {
    // Set SG 2 mode => SG1 = ACTIVE, SG2 = INACTIVE
    digitalWrite(config.sg1Pin, sgActiveState());
    digitalWrite(config.sg2Pin, sgInactiveState());
    sgMode = 2;
    client.publish("espaltherma/sg/state", "2");
    mqttSerial.println("Set SG mode to 2 - Recommended ON");
  }
  else if (payload[0] == '3')
  {
    // Set SG 3 mode => SG1 = ACTIVE, SG2 = ACTIVE
    digitalWrite(config.sg1Pin, sgActiveState());
    digitalWrite(config.sg2Pin, sgActiveState());
    sgMode = 3;
    client.publish("espaltherma/sg/state", "3");
    mqttSerial.println("Set SG mode to 3 - Forced ON");
  }
  else
  {
    mqttSerial.printf("Unknown message: %s\n", payload);
  }
}

void callbackSafety(byte *payload, unsigned int length)
{
  payload[length] = '\0';

  if (payload[0] == '0')
  {
    // Set Safety relay to OFF
    digitalWrite(config.safetyPin, !safetyActiveState());
    safetyActive = false;
    client.publish("espaltherma/SAFETY_STATE", "0", true);
  }
  else if (payload[0] == '1')
  {
    // Set Safety relay to ON
    digitalWrite(config.safetyPin, safetyActiveState());
    safetyActive = true;
    client.publish("espaltherma/SAFETY_STATE", "1", true);
  }
  else
  {
    mqttSerial.printf("Unknown message: %s\n", payload);
  }
}

void callbackDebugSerial(byte *payload, unsigned int length)
{
  payload[length] = '\0';
  
  serialLock(); // The heat pump task must not query while we use the link
  // Send message to serial port
  MySerial.write(payload, length);
  MySerial.flush();
  
  // Wait for response with timeout
  unsigned long startTime = millis();
  const unsigned long timeout = 1000; // 1 second timeout
  byte responseBuffer[256]; // Buffer to store raw binary data
  int responseLength = 0;
  
  while (millis() - startTime < timeout && responseLength < sizeof(responseBuffer))
  {
    if (MySerial.available())
    {
      responseBuffer[responseLength++] = MySerial.read();
      
      // Reset timeout if we're still receiving data
      startTime = millis();
    }
    
    // Allow other operations during waiting
    client.loop();
    ArduinoOTA.handle();
    yield();
  }
  serialUnlock();
  
  // Publish response if we got any data, encoded as hex string
  if (responseLength > 0)
  {
    String hexResponse = "";
    for (int i = 0; i < responseLength; i++)
    {
      if (i > 0) hexResponse += " ";
      if (responseBuffer[i] < 0x10) hexResponse += "0";
      hexResponse += String(responseBuffer[i], HEX);
    }
    hexResponse.toUpperCase();
    
    client.publish("espaltherma/serialRX", hexResponse.c_str());
  }
}


void callback(char *topic, byte *payload, unsigned int length)
{
  mqttSerial.printf("Message arrived [%s] : %s\n", topic, payload);

  if (strcmp(topic, "espaltherma/POWER") == 0)
  {
    callbackTherm(payload, length);
  }
  else if (strcmp(topic, "espaltherma/detect/run") == 0)
  {
    mqttSerial.println("Heat pump survey requested");
    requestSurvey();
  }
  else if (config.sg1Pin >= 0 && strcmp(topic, "espaltherma/sg/set") == 0)
  {
    callbackSg(payload, length);
  }
  else if (config.safetyPin >= 0 && strcmp(topic, "espaltherma/SAFETY") == 0)
  {
    callbackSafety(payload, length);
  }
  else if (config.debugSerial && strcmp(topic, "espaltherma/serialTX") == 0)
  {
    callbackDebugSerial(payload, length);
  }

  else
  {
    mqttSerial.printf("Unknown topic: %s\n", topic);
  }
}

// Streams a device-discovery payload chunk straight to the MQTT socket (used as a DiscoverySink).
static void mqttDiscoverySink(void *ctx, const char *data, size_t len)
{
  PubSubClient *c = static_cast<PubSubClient *>(ctx);
  if (!mqttWriteFailed && c->write(reinterpret_cast<const uint8_t *>(data), len) != len)
    mqttWriteFailed = true; // the rest would wait 10s per write, with the values locked
}

void publishHomeAssistantDeviceDiscovery()
{
  valuesLock(); // the heat pump task may be changing the values
  const LabelDef *labels = converter.activeLabels;
  const size_t count = converter.activeCount;
  const char *topic = "homeassistant/device/espaltherma-mqtt-discovery/config";

  // First pass: compute the exact payload length, required up-front for the MQTT fixed header.
  size_t payloadLen = streamDeviceDiscoveryPayload(labels, count, nullptr, nullptr);

  mqttSerial.printf("Sending HA discovery: %u sensors, %u bytes\n",
                    (unsigned)(count + 3), (unsigned)payloadLen);

  // Stream the payload directly to the socket. PubSubClient::write() writes straight to the
  // TCP/TLS client, so the MQTT buffer only needs to hold the topic+header. This avoids the large
  // contiguous setBufferSize() allocation that previously ran out of heap on bigger definitions.
  if (!client.beginPublish(topic, payloadLen, true))
  {
    valuesUnlock();
    mqttSerial.println("Error starting device discovery publish!");
    return;
  }

  streamDeviceDiscoveryPayload(labels, count, mqttDiscoverySink, &client);

  client.endPublish();
  valuesUnlock();
}
