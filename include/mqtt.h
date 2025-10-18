#include <PubSubClient.h>
#include <EEPROM.h>
#include "restart.h"
#include "comm.h"

#define MQTT_attr "espaltherma/ATTR"
#define MQTT_lwt "espaltherma/LWT"

#define EEPROM_CHK 1
#define EEPROM_HEAT_STATE 0

#ifdef PIN_COOL
#define EEPROM_COOL_STATE 2
#endif

#define MQTT_attr "espaltherma/ATTR"
#define MQTT_lwt "espaltherma/LWT"

enum Mode { heating, cooling };

#ifdef JSONTABLE
char jsonbuff[MAX_MSG_SIZE] = "[{\0";
#else
char jsonbuff[MAX_MSG_SIZE] = "{\0";
#endif

#ifdef MQTT_ENCRYPTED
#include <WiFiClientSecure.h>
WiFiClientSecure espClient;
#else
WiFiClient espClient;
#endif
PubSubClient client(espClient);

void sendValues()
{
  Serial.printf("Sending values in MQTT.\n");
#ifdef ARDUINO_M5Stick_C_Plus2
  //Add Power values
  // getBatteryVoltage returns battery voltage [mV] as an int16_t
  float batteryVoltage = (float) M5.Power.getBatteryVoltage() / 1000; // convert to V as a float
  snprintf(jsonbuff + strlen(jsonbuff),MAX_MSG_SIZE - strlen(jsonbuff) , "\"%s\":\"%.3gV\",", "M5BatV", batteryVoltage);
#elif ARDUINO_M5Stick_C
  //Add M5 APX values
  snprintf(jsonbuff + strlen(jsonbuff),MAX_MSG_SIZE - strlen(jsonbuff) , "\"%s\":\"%.3gV\",\"%s\":\"%gmA\",", "M5VIN", M5.Axp.GetVinVoltage(),"M5AmpIn", M5.Axp.GetVinCurrent());
  snprintf(jsonbuff + strlen(jsonbuff),MAX_MSG_SIZE - strlen(jsonbuff) , "\"%s\":\"%.3gV\",\"%s\":\"%gmA\",", "M5BatV", M5.Axp.GetBatVoltage(),"M5BatCur", M5.Axp.GetBatCurrent());
  snprintf(jsonbuff + strlen(jsonbuff),MAX_MSG_SIZE - strlen(jsonbuff) , "\"%s\":\"%.3gmW\",", "M5BatPwr", M5.Axp.GetBatPower());
#endif
  snprintf(jsonbuff + strlen(jsonbuff),MAX_MSG_SIZE - strlen(jsonbuff) , "\"%s\":\"%ddBm\",", "WifiRSSI", WiFi.RSSI());
  snprintf(jsonbuff + strlen(jsonbuff),MAX_MSG_SIZE - strlen(jsonbuff) , "\"%s\":\"%d\",", "FreeMem", ESP.getFreeHeap());
  jsonbuff[strlen(jsonbuff) - 1] = '}';
#ifdef JSONTABLE
  strcat(jsonbuff,"]");
#endif
  client.publish(MQTT_attr, jsonbuff);
#ifdef JSONTABLE
  strcpy(jsonbuff, "[{\0");
#else
  strcpy(jsonbuff, "{\0");
#endif
}

void saveEEPROM(uint8_t address, uint8_t state){
    EEPROM.write(address,state);
    EEPROM.commit();
}

void readEEPROM(){
  if ('R' == EEPROM.read(EEPROM_CHK)){
    digitalWrite(PIN_THERM,EEPROM.read(EEPROM_HEAT_STATE));
    mqttSerial.printf("Restoring previous heating state: %s",(EEPROM.read(EEPROM_HEAT_STATE) == PIN_THERM_ACTIVE_STATE)? "On":"Off" );
#ifdef PIN_COOL
    digitalWrite(PIN_COOL,EEPROM.read(EEPROM_COOL_STATE));
    mqttSerial.printf("Restoring previous cooling state: %s",(EEPROM.read(EEPROM_COOL_STATE) == PIN_THERM_ACTIVE_STATE)? "On":"Off" );
#endif
  }
  else{
    mqttSerial.printf("EEPROM not initialized (%d). Initializing...",EEPROM.read(EEPROM_CHK));
    EEPROM.write(EEPROM_CHK,'R');
    EEPROM.write(EEPROM_HEAT_STATE,!PIN_THERM_ACTIVE_STATE);
#ifdef PIN_COOL
    EEPROM.write(EEPROM_COOL_STATE,!PIN_THERM_ACTIVE_STATE);
#endif
    EEPROM.commit();
    digitalWrite(PIN_THERM,!PIN_THERM_ACTIVE_STATE);
#ifdef PIN_COOL
    digitalWrite(PIN_COOL,!PIN_THERM_ACTIVE_STATE);
#endif
  }
}

void reconnectMqtt()
{
  // Loop until we're reconnected
  int i = 0;
  while (!client.connected())
  {
    Serial.print("Attempting MQTT connection...");

    if (client.connect("ESPAltherma-dev", MQTT_USERNAME, MQTT_PASSWORD, MQTT_lwt, 0, true, "Offline"))
    {
      Serial.println("connected!");
      client.publish("homeassistant/sensor/espAltherma/config", "{\"name\":\"AlthermaSensors\",\"stat_t\":\"~/LWT\",\"avty_t\":\"~/LWT\",\"pl_avail\":\"Online\",\"pl_not_avail\":\"Offline\",\"uniq_id\":\"espaltherma\",\"device\":{\"identifiers\":[\"ESPAltherma\"]}, \"~\":\"espaltherma\",\"json_attr_t\":\"~/ATTR\"}", true);
      client.publish(MQTT_lwt, "Online", true);
      client.publish("homeassistant/switch/espAltherma/config", "{\"name\":\"Altherma\",\"cmd_t\":\"~/POWER\",\"stat_t\":\"~/STATE\",\"pl_off\":\"OFF\",\"pl_on\":\"ON\",\"~\":\"espaltherma\"}", true);

      // Subscribe
      client.subscribe("espaltherma/POWER");

#ifdef PIN_COOL
      client.publish("homeassistant/switch/espAltherma_cool/config", "{\"name\":\"Altherma_cooling\",\"cmd_t\":\"~/COOLING\",\"stat_t\":\"~/COOL_STATE\",\"pl_off\":\"OFF\",\"pl_on\":\"ON\",\"~\":\"espaltherma\"}", true);
      client.subscribe("espaltherma/COOLING");
#endif

#ifdef PIN_SG1
      // Smart Grid
      client.publish("homeassistant/select/espAltherma/sg/config", "{\"availability\":[{\"topic\":\"espaltherma/LWT\",\"payload_available\":\"Online\",\"payload_not_available\":\"Offline\"}],\"availability_mode\":\"all\",\"unique_id\":\"espaltherma_sg\",\"device\":{\"identifiers\":[\"ESPAltherma\"],\"manufacturer\":\"ESPAltherma\",\"model\":\"M5StickC PLUS ESP32-PICO\",\"name\":\"ESPAltherma\"},\"icon\":\"mdi:solar-power\",\"name\":\"EspAltherma Smart Grid\",\"command_topic\":\"espaltherma/sg/set\",\"command_template\":\"{% if value == 'Free Running' %} 0 {% elif value == 'Forced Off' %} 1 {% elif value == 'Recommended On' %} 2 {% elif value == 'Forced On' %} 3 {% else %} 0 {% endif %}\",\"options\":[\"Free Running\",\"Forced Off\",\"Recommended On\",\"Forced On\"],\"state_topic\":\"espaltherma/sg/state\",\"value_template\":\"{% set mapper = { '0':'Free Running', '1':'Forced Off', '2':'Recommended On', '3':'Forced On' } %} {% set word = mapper[value] %} {{ word }}\"}", true);
      client.subscribe("espaltherma/sg/set");
      client.publish("espaltherma/sg/state", "0");
#endif

#ifdef SAFETY_RELAY_PIN
      // Safety relay
      client.publish("homeassistant/switch/espAltherma/safety/config", "{\"name\":\"Altherma Safety\",\"cmd_t\":\"~/SAFETY\",\"stat_t\":\"~/SAFETY_STATE\",\"pl_off\":\"0\",\"pl_on\":\"1\",\"~\":\"espaltherma\"}", true);
      client.subscribe("espaltherma/SAFETY");
#endif

#ifdef DEBUG_SERIAL
      // DebugSerial - MQTT<>Serial gateway
      client.subscribe("espaltherma/serialTX");
#endif

#ifndef PIN_SG1
      // Publish empty retained message so discovered entities are removed from HA
      client.publish("homeassistant/select/espAltherma/sg/config", "", true);
#endif
    }
    else
    {
      Serial.printf("failed, rc=%d, try again in 5 seconds", client.state());
      unsigned long start = millis();
      while (millis() < start + 5000)
      {
        ArduinoOTA.handle();
      }

      if (i++ == 100) {
        Serial.printf("Tried for 500 sec, rebooting now.");
        restart_board();
      }
    }
  }
}

void setHeatPumpState(Mode mode, bool turn_on) {
  // It is possible that the state is inversed in comparison to the original code for heating only, not sure why
  uint8_t state = (turn_on) ? PIN_THERM_ACTIVE_STATE : !PIN_THERM_ACTIVE_STATE;
  std::string value = (turn_on) ? "ON" : "OFF";
  
  std::string topic = (mode == heating) ? "STATE" : "COOL_STATE";
  unsigned int pin = (mode == heating) ? PIN_THERM : PIN_COOL;
  uint8_t eeprom_address = (mode == heating) ? EEPROM_HEAT_STATE : EEPROM_COOL_STATE;

  digitalWrite(pin, state);
  saveEEPROM(eeprom_address, state);

  client.publish(("espaltherma/" + topic).c_str(), value.c_str(), true);

  std::string logmessage = "Turned " + topic + " " + value;
  mqttSerial.println(logmessage.c_str());
}

void callbackHeatPumpState(byte *payload, unsigned int length, Mode mode)
{
  payload[length] = '\0';

  // Is it ON or OFF?
  // Ok I'm not super proud of this, but it works :p
  if (payload[1] == 'F')
  { //turn off
    setHeatPumpState(mode, false);
  }
  else if (payload[1] == 'N')
  { //turn on
    if (mode == heating) {
#ifdef PIN_COOL
      setHeatPumpState(cooling, false);
#endif
      setHeatPumpState(heating, true);
    }
#ifdef PIN_COOL
    else {
      setHeatPumpState(heating, false);
      setHeatPumpState(cooling, true);
    }
#endif
  }
  else if (payload[0] == 'R')//R(eset/eboot)
  {
    mqttSerial.println("Rebooting");
    delay(100);
    restart_board();
  }
  else
  {
    Serial.printf("Unknown message: %s\n", payload);
  }
}

#ifdef PIN_SG1
//Smartgrid callbacks
void callbackSg(byte *payload, unsigned int length)
{
  payload[length] = '\0';

  if (payload[0] == '0')
  {
    // Set SG 0 mode => SG1 = INACTIVE, SG2 = INACTIVE
    digitalWrite(PIN_SG1, SG_RELAY_INACTIVE_STATE);
    digitalWrite(PIN_SG2, SG_RELAY_INACTIVE_STATE);
    client.publish("espaltherma/sg/state", "0");
    Serial.println("Set SG mode to 0 - Normal operation");
  }
  else if (payload[0] == '1')
  {
    // Set SG 1 mode => SG1 = INACTIVE, SG2 = ACTIVE
    digitalWrite(PIN_SG1, SG_RELAY_INACTIVE_STATE);
    digitalWrite(PIN_SG2, SG_RELAY_ACTIVE_STATE);
    client.publish("espaltherma/sg/state", "1");
    Serial.println("Set SG mode to 1 - Forced OFF");
  }
  else if (payload[0] == '2')
  {
    // Set SG 2 mode => SG1 = ACTIVE, SG2 = INACTIVE
    digitalWrite(PIN_SG1, SG_RELAY_ACTIVE_STATE);
    digitalWrite(PIN_SG2, SG_RELAY_INACTIVE_STATE);
    client.publish("espaltherma/sg/state", "2");
    Serial.println("Set SG mode to 2 - Recommended ON");
  }
  else if (payload[0] == '3')
  {
    // Set SG 3 mode => SG1 = ACTIVE, SG2 = ACTIVE
    digitalWrite(PIN_SG1, SG_RELAY_ACTIVE_STATE);
    digitalWrite(PIN_SG2, SG_RELAY_ACTIVE_STATE);
    client.publish("espaltherma/sg/state", "3");
    Serial.println("Set SG mode to 3 - Forced ON");
  }
  else
  {
    Serial.printf("Unknown message: %s\n", payload);
  }
}
#endif

#ifdef SAFETY_RELAY_PIN
void callbackSafety(byte *payload, unsigned int length)
{
  payload[length] = '\0';

  if (payload[0] == '0')
  {
    // Set Safety relay to OFF
    digitalWrite(SAFETY_RELAY_PIN, !SAFETY_RELAY_ACTIVE_STATE);
    client.publish("espaltherma/SAFETY_STATE", "0", true);
  }
  else if (payload[0] == '1')
  {
    // Set Safety relay to ON
    digitalWrite(SAFETY_RELAY_PIN, SAFETY_RELAY_ACTIVE_STATE);
    client.publish("espaltherma/SAFETY_STATE", "1", true);
  }
  else
  {
    Serial.printf("Unknown message: %s\n", payload);
  }
}
#endif

#ifdef DEBUG_SERIAL
void callbackDebugSerial(byte *payload, unsigned int length)
{
  payload[length] = '\0';
  
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
#endif


void callback(char *topic, byte *payload, unsigned int length)
{
  Serial.printf("Message arrived [%s] : %s\n", topic, payload);

  if (strcmp(topic, "espaltherma/POWER") == 0)
  {
    callbackHeatPumpState(payload, length, heating);
  }
#ifdef PIN_COOL
  else if (strcmp(topic, "espaltherma/COOLING") == 0)
  {
    callbackHeatPumpState(payload, length, cooling);
  }
#endif
#ifdef PIN_SG1
  else if (strcmp(topic, "espaltherma/sg/set") == 0)
  {
    callbackSg(payload, length);
  }
#endif
#ifdef SAFETY_RELAY_PIN
  else if (strcmp(topic, "espaltherma/SAFETY") == 0)
  {
    callbackSafety(payload, length);
  }
#endif
#ifdef DEBUG_SERIAL
  else if (strcmp(topic, "espaltherma/serialTX") == 0)
  {
    callbackDebugSerial(payload, length);
  }
#endif

  else
  {
    Serial.printf("Unknown topic: %s\n", topic);
  }
}
