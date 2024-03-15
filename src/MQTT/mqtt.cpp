#include "mqtt.hpp"

WiFiClientSecure espClient;
PubSubClient client(espClient);

String subscribeHeatingTopic = "";
String subscribeCoolingTopic = "";
String subscribeSGTopic = "";
String subscribeCANTopic = "";
String subscribeSafetyTopic = "";
String subscribePowerTopic = "";

String publishHeatingTopic = "";
String publishCoolingTopic = "";
String publishSGTopic = "";
String publishSafetyTopic = "";
String publishPowerTopic = "";
String publishAttrTopic = "";
String publishLWTTopic = "";

char jsonbuff[MAX_MSG_SIZE];
uint8_t SG_RELAY_ACTIVE_STATE;
uint8_t SG_RELAY_INACTIVE_STATE;

std::function<void(const String &label, const char *payload, const uint32_t length)> callbackCAN;

void initMQTT()
{
  // allow invalid SSL certificates on MQTT
  espClient.setInsecure();
  espClient.setTimeout(5);

  client.setServer(config->MQTT_SERVER.c_str(), config->MQTT_PORT);
  client.setBufferSize(MAX_MSG_SIZE); // to support large json message
  client.setCallback(callback);

  createEmptyJSONBuffer();

  SG_RELAY_ACTIVE_STATE = (config->SG_RELAY_HIGH_TRIGGER == true) ? HIGH : LOW;
  SG_RELAY_INACTIVE_STATE = SG_RELAY_ACTIVE_STATE == HIGH ? LOW : HIGH;

  debugSerial.printf("SG Active State: %u\n", SG_RELAY_ACTIVE_STATE);
  debugSerial.printf("SG Inactive State: %u\n", SG_RELAY_INACTIVE_STATE);
}

void createEmptyJSONBuffer()
{
  if(config->MQTT_USE_JSONTABLE)
    strcpy(jsonbuff, "[{\0");
  else
    strcpy(jsonbuff, "{\0");
}

// converts to string and add the value to the JSON message
void updateValues(ParameterDef *labelDef)
{
  bool alpha = false;
  for (size_t j = 0; j < strlen(labelDef->asString); j++) {
    char c = labelDef->asString[j];
    if (!isdigit(c) && c!='.' && !(c=='-' && j==0)) {
      alpha = true;
      break;
    }
  }

  if(config->MQTT_USE_ONETOPIC) {
    client.publish((config->MQTT_TOPIC_NAME + config->MQTT_ONETOPIC_NAME + labelDef->label).c_str(), labelDef->asString);
    return;
  }

  if(alpha) {
    snprintf(jsonbuff + strlen(jsonbuff), MAX_MSG_SIZE - strlen(jsonbuff), "\"%s\":\"%s\",", labelDef->label.c_str(), labelDef->asString);
  } else { // number, no quotes
    snprintf(jsonbuff + strlen(jsonbuff), MAX_MSG_SIZE - strlen(jsonbuff), "\"%s\":%s,", labelDef->label.c_str(), labelDef->asString);
  }
}

void sendValues()
{
  debugSerial.printf("Sending values in MQTT.\n");
#ifdef ARDUINO_M5Stick_C_Plus2
  //Add Power values
  // getBatteryVoltage returns battery voltage [mV] as an int16_t
  float batteryVoltage = (float) M5.Power.getBatteryVoltage() / 1000; // convert to V as a float
  snprintf(jsonbuff + strlen(jsonbuff), MAX_MSG_SIZE - strlen(jsonbuff), "\"%s\":\"%.3gV\",", "M5BatV", batteryVoltage);
#elif ARDUINO_M5Stick_C
  // add M5 APX values
  snprintf(jsonbuff + strlen(jsonbuff), MAX_MSG_SIZE - strlen(jsonbuff), "\"%s\":\"%.3gV\",\"%s\":\"%gmA\",", "M5VIN", M5.Axp.GetVinVoltage(),"M5AmpIn", M5.Axp.GetVinCurrent());
  snprintf(jsonbuff + strlen(jsonbuff), MAX_MSG_SIZE - strlen(jsonbuff), "\"%s\":\"%.3gV\",\"%s\":\"%gmA\",", "M5BatV", M5.Axp.GetBatVoltage(),"M5BatCur", M5.Axp.GetBatCurrent());
  snprintf(jsonbuff + strlen(jsonbuff), MAX_MSG_SIZE - strlen(jsonbuff), "\"%s\":\"%.3gmW\",", "M5BatPwr", M5.Axp.GetBatPower());
#endif
  snprintf(jsonbuff + strlen(jsonbuff), MAX_MSG_SIZE - strlen(jsonbuff), "\"%s\":\"%ddBm\",", "WifiRSSI", WiFi.RSSI());
  snprintf(jsonbuff + strlen(jsonbuff), MAX_MSG_SIZE - strlen(jsonbuff), "\"%s\":\"%d\",", "FreeMem", ESP.getFreeHeap());

  jsonbuff[strlen(jsonbuff) - 1] = '}';

  if(config->MQTT_USE_JSONTABLE)
    strcat(jsonbuff,"]");

  client.publish(publishAttrTopic.c_str(), jsonbuff);

  createEmptyJSONBuffer();
}

void reconnectMqtt()
{
  //in case loopback as server is set, skip connecting (debug purpose)
  if(config->MQTT_SERVER.compareTo("127.0.0.1") == 0 || config->MQTT_SERVER.compareTo("localhost") == 0)
  {
    debugSerial.print("Found loopback MQTT server, skiping connection...\n");
    return;
  }

  subscribeHeatingTopic = config->MQTT_TOPIC_NAME + MQTT_TOPIC_SUB_HEATING;
  subscribeCoolingTopic = config->MQTT_TOPIC_NAME + MQTT_TOPIC_SUB_COOLING;
  subscribeSGTopic      = config->MQTT_TOPIC_NAME + MQTT_TOPIC_SUB_SG;
  subscribePowerTopic   = config->MQTT_TOPIC_NAME + MQTT_TOPIC_SUB_POWER;
  if(config->CAN_ENABLED) {
    subscribeCANTopic   = config->MQTT_TOPIC_NAME + "SET/" + config->CAN_CONFIG->CAN_MQTT_TOPIC_NAME;
  }
  subscribeSafetyTopic = config->MQTT_TOPIC_NAME + MQTT_TOPIC_SUB_SAFETY;

  publishHeatingTopic   = config->MQTT_TOPIC_NAME + MQTT_TOPIC_PUB_HEATING;
  publishCoolingTopic   = config->MQTT_TOPIC_NAME + MQTT_TOPIC_PUB_COOLING;
  publishSGTopic        = config->MQTT_TOPIC_NAME + MQTT_TOPIC_PUB_SG;
  publishSafetyTopic    = config->MQTT_TOPIC_NAME + MQTT_TOPIC_PUB_SAFETY;
  publishPowerTopic     = config->MQTT_TOPIC_NAME + MQTT_TOPIC_PUB_POWER;
  publishAttrTopic      = config->MQTT_TOPIC_NAME + MQTT_TOPIC_PUB_ATTR;
  publishLWTTopic       = config->MQTT_TOPIC_NAME + MQTT_TOPIC_PUB_LWT;

  // loop until we're reconnected
  int i = 0;
  while (!client.connected()) {
    debugSerial.print("Attempting MQTT connection with ID: ");

    String id = "ESPAltherma-";
    id += WiFi.macAddress().substring(6);

    debugSerial.println(id);

    if (client.connect(id.c_str(), config->MQTT_USERNAME.c_str(), config->MQTT_PASSWORD.c_str(), publishLWTTopic.c_str(), 0, true, "Offline")) {
      debugSerial.println("connected!");

      // TODO update homeassistant config to publish correct informations
      client.publish("homeassistant/sensor/espAltherma/config", "{\"name\":\"AlthermaSensors\",\"stat_t\":\"~/STATESENS\",\"avty_t\":\"~/LWT\",\"pl_avail\":\"Online\",\"pl_not_avail\":\"Offline\",\"uniq_id\":\"espaltherma\",\"device\":{\"identifiers\":[\"ESPAltherma\"]}, \"~\":\"espaltherma\",\"json_attr_t\":\"~/ATTR\"}", true);
      client.publish(publishLWTTopic.c_str(), "Online", true);
      client.publish("homeassistant/switch/espAltherma/config", "{\"name\":\"Altherma Heating\",\"cmd_t\":\"~/SET/HEATING\",\"stat_t\":\"~/STATE/HEATING\",\"pl_off\":\"OFF\",\"pl_on\":\"ON\",\"~\":\"espaltherma\"}", true);
      client.publish("homeassistant/switch/espAltherma/config", "{\"name\":\"Altherma Cooling\",\"cmd_t\":\"~/SET/COOLING\",\"stat_t\":\"~/STATE/COOLING\",\"pl_off\":\"OFF\",\"pl_on\":\"ON\",\"~\":\"espaltherma\"}", true);
      client.publish("homeassistant/switch/espAltherma/config", "{\"name\":\"Altherma SmartGrid\",\"cmd_t\":\"~/SET/SG\",\"stat_t\":\"~/STATE/SG\",\"pl_off\":\"OFF\",\"pl_on\":\"ON\",\"~\":\"espaltherma\"}", true);

      // subscribe
      client.subscribe(subscribeHeatingTopic.c_str());
      client.subscribe(subscribeCoolingTopic.c_str());
      client.subscribe(subscribePowerTopic.c_str());

      debugSerial.println("Subscribed to following topics:");
      debugSerial.println(subscribeHeatingTopic);
      debugSerial.println(subscribeCoolingTopic);
      debugSerial.println(subscribePowerTopic);

      if(config->SG_ENABLED) {
        // Smart Grid
        client.publish("homeassistant/select/espAltherma/sg/config", "{\"availability\":[{\"topic\":\"espaltherma/LWT\",\"payload_available\":\"Online\",\"payload_not_available\":\"Offline\"}],\"availability_mode\":\"all\",\"unique_id\":\"espaltherma_sg\",\"device\":{\"identifiers\":[\"ESPAltherma\"],\"manufacturer\":\"ESPAltherma\",\"model\":\"M5StickC PLUS ESP32-PICO\",\"name\":\"ESPAltherma\"},\"icon\":\"mdi:solar-power\",\"name\":\"EspAltherma Smart Grid\",\"command_topic\":\"espaltherma/sg/set\",\"command_template\":\"{% if value == 'Free Running' %} 0 {% elif value == 'Forced Off' %} 1 {% elif value == 'Recommended On' %} 2 {% elif value == 'Forced On' %} 3 {% else %} 0 {% endif %}\",\"options\":[\"Free Running\",\"Forced Off\",\"Recommended On\",\"Forced On\"],\"state_topic\":\"espaltherma/sg/state\",\"value_template\":\"{% set mapper = { '0':'Free Running', '1':'Forced Off', '2':'Recommended On', '3':'Forced On' } %} {% set word = mapper[value] %} {{ word }}\"}", true);
        client.subscribe(subscribeSGTopic.c_str());

        debugSerial.println(subscribeSGTopic);
      } else {
        // Publish empty retained message so discovered entities are removed from HA
        client.publish("homeassistant/select/espAltherma/sg/config", "", true);
      }

      if(config->CAN_ENABLED) {
        client.subscribe((subscribeCANTopic + "#").c_str());
        debugSerial.println(subscribeCANTopic);
      }

      if(config->SAFETY_ENABLED) {
        client.publish("homeassistant/switch/espAltherma/safety/config", "{\"name\":\"Altherma Safety\",\"cmd_t\":\"~/SET/SAFETY\",\"stat_t\":\"~/STATE/SAFETY\",\"pl_off\":\"0\",\"pl_on\":\"1\",\"~\":\"espaltherma\"}", true);
        client.subscribe(subscribeSafetyTopic.c_str());
        debugSerial.println(subscribeSafetyTopic);
      }
    } else {
      debugSerial.printf("failed, rc=%d, try again in 5 seconds", client.state());
      unsigned long start = millis();
      while (millis() < start + 5000) { }

      if (i++ == 100) {
        debugSerial.printf("Tried for 500 sec, rebooting now.");
        restart_board();
      }
    }
  }
}

void callbackHeating(byte *payload, unsigned int length)
{
  payload[length] = '\0';

  // is it ON or OFF?
  // ok I'm not super proud of this, but it works :p
  if (payload[1] == 'F') {
    // turn off
    digitalWrite(config->PIN_HEATING, HIGH);
    savePersistence();
    client.publish(publishHeatingTopic.c_str(), "OFF", true);
    debugSerial.println("Heating turned OFF");
  } else if (payload[1] == 'N') {
    // turn on
    digitalWrite(config->PIN_HEATING, LOW);
    savePersistence();
    client.publish(publishHeatingTopic.c_str(), "ON", true);
    debugSerial.println("Heating turned ON");
  } else {
    debugSerial.printf("Unknown message: %s\n", payload);
  }
}

void callbackPower(byte *payload, unsigned int length)
{
  payload[length] = '\0';

  if (payload[0] == 'R') {
    // R(eset/eboot)
    debugSerial.println("Rebooting");
    delay(100);
    restart_board();
  } else {
    debugSerial.printf("Unknown message: %s\n", payload);
  }
}

void callbackCooling(byte *payload, unsigned int length)
{
  payload[length] = '\0';

  // is it ON or OFF?
  // ok I'm not super proud of this, but it works :p
  if (payload[1] == 'F') {
    // turn off
    digitalWrite(config->PIN_COOLING, HIGH);
    savePersistence();
    client.publish(publishCoolingTopic.c_str(), "OFF", true);
    debugSerial.println("Cooling turned OFF");
  } else if (payload[1] == 'N') {
    //turn on
    digitalWrite(config->PIN_COOLING, LOW);
    savePersistence();
    client.publish(publishCoolingTopic.c_str(), "ON", true);
    debugSerial.println("Cooling turned ON");
  } else {
    debugSerial.printf("Unknown message: %s\n", payload);
  }
}

// Smartgrid callbacks
void callbackSg(byte *payload, unsigned int length)
{
  payload[length] = '\0';

  if (payload[0] == '0') {
    // set SG 0 mode => SG1 = INACTIVE, SG2 = INACTIVE
    digitalWrite(config->PIN_SG1, SG_RELAY_INACTIVE_STATE);
    digitalWrite(config->PIN_SG2, SG_RELAY_INACTIVE_STATE);
    client.publish(publishSGTopic.c_str(), "0");
    debugSerial.println("Set SG mode to 0 - Normal operation");
  } else if (payload[0] == '1') {
    // set SG 1 mode => SG1 = INACTIVE, SG2 = ACTIVE
    digitalWrite(config->PIN_SG1, SG_RELAY_INACTIVE_STATE);
    digitalWrite(config->PIN_SG2, SG_RELAY_ACTIVE_STATE);
    client.publish(publishSGTopic.c_str(), "1");
    debugSerial.println("Set SG mode to 1 - Forced OFF");
  } else if (payload[0] == '2') {
    // set SG 2 mode => SG1 = ACTIVE, SG2 = INACTIVE
    digitalWrite(config->PIN_SG1, SG_RELAY_ACTIVE_STATE);
    digitalWrite(config->PIN_SG2, SG_RELAY_INACTIVE_STATE);
    client.publish(publishSGTopic.c_str(), "2");
    debugSerial.println("Set SG mode to 2 - Recommended ON");
  } else if (payload[0] == '3') {
    // set SG 3 mode => SG1 = ACTIVE, SG2 = ACTIVE
    digitalWrite(config->PIN_SG1, SG_RELAY_ACTIVE_STATE);
    digitalWrite(config->PIN_SG2, SG_RELAY_ACTIVE_STATE);
    client.publish(publishSGTopic.c_str(), "3");
    debugSerial.println("Set SG mode to 3 - Forced ON");
  } else {
    debugSerial.printf("Unknown message: %s\n", payload);
  }
}

void callbackSafety(byte *payload, unsigned int length)
{
  payload[length] = '\0';

  if (payload[0] == '0') {
    // Set Safety relay to OFF
    digitalWrite(config->PIN_SAFETY, LOW);
    client.publish(publishSafetyTopic.c_str(), "0", true);
  } else if (payload[0] == '1') {
    // Set Safety relay to ON
    digitalWrite(config->PIN_SAFETY, HIGH);
    client.publish(publishSafetyTopic.c_str(), "1", true);
  } else {
    Serial.printf("Unknown message: %s\n", payload);
  }
}

void callback(char *topic, byte *payload, unsigned int length)
{
  char payloadText[length+1];
  for (int i=0;i<length;i++) {
    payloadText[i] = (char)payload[i];
  }
  payloadText[length] = '\0';

  debugSerial.printf("Message arrived [%s] : %s\n", topic, payloadText);

  if (config->HEATING_ENABLED && subscribeHeatingTopic == topic) {
    callbackHeating(payload, length);
  } else if (config->COOLING_ENABLED && subscribeCoolingTopic == topic) {
    callbackCooling(payload, length);
  } else if (subscribePowerTopic == topic) {
    callbackPower(payload, length);
  } else if (config->SG_ENABLED && subscribeSGTopic == topic) {
    callbackSg(payload, length);
  } else if (config->CAN_ENABLED && String(topic).startsWith(subscribeCANTopic)) {
    if(callbackCAN != nullptr)
      callbackCAN(String(topic).substring(subscribeCANTopic.length()), payloadText, length);
  } else if (config->SAFETY_ENABLED && subscribeSafetyTopic == topic) {
    callbackSafety(payload, length);
  } else {
    debugSerial.printf("Unknown topic: %s\n", topic);
  }
}