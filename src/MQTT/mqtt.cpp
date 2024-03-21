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
String publishCANTopic = "";
String publishSafetyTopic = "";
String publishPowerTopic = "";
String publishAttrTopic = "";
String publishLWTTopic = "";

int canTopicLength;
char jsonbuff[MAX_MSG_SIZE];
bool blockReconnects;
uint8_t reconnectionAtempts;
bool reconnectionInProgress = false;
unsigned long reconnectStartMillis;
uint8_t SG_RELAY_ACTIVE_STATE;
uint8_t SG_RELAY_INACTIVE_STATE;

void initMQTT()
{
  // allow invalid SSL certificates on MQTT
  espClient.setInsecure();
  espClient.setTimeout(5);

  client.setServer(config->MQTT_SERVER.c_str(), config->MQTT_PORT);
  client.setBufferSize(MAX_MSG_SIZE); // to support large json message

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

void connectMqtt()
{
  if(config->MQTT_SERVER.compareTo("127.0.0.1") == 0 || config->MQTT_SERVER.compareTo("localhost") == 0) {
    blockReconnects = true;
    debugSerial.print("Found loopback MQTT server, skiping connection...\n");
    return;
  }

  subscribeHeatingTopic = config->MQTT_TOPIC_NAME + MQTT_TOPIC_SUB_HEATING;
  subscribeCoolingTopic = config->MQTT_TOPIC_NAME + MQTT_TOPIC_SUB_COOLING;
  subscribeSGTopic      = config->MQTT_TOPIC_NAME + MQTT_TOPIC_SUB_SG;
  subscribePowerTopic   = config->MQTT_TOPIC_NAME + MQTT_TOPIC_SUB_POWER;
  subscribeSafetyTopic  = config->MQTT_TOPIC_NAME + MQTT_TOPIC_SUB_SAFETY;

  publishHeatingTopic   = config->MQTT_TOPIC_NAME + MQTT_TOPIC_PUB_HEATING;
  publishCoolingTopic   = config->MQTT_TOPIC_NAME + MQTT_TOPIC_PUB_COOLING;
  publishSGTopic        = config->MQTT_TOPIC_NAME + MQTT_TOPIC_PUB_SG;
  publishSafetyTopic    = config->MQTT_TOPIC_NAME + MQTT_TOPIC_PUB_SAFETY;
  publishPowerTopic     = config->MQTT_TOPIC_NAME + MQTT_TOPIC_PUB_POWER;
  publishAttrTopic      = config->MQTT_TOPIC_NAME + MQTT_TOPIC_PUB_ATTR;
  publishLWTTopic       = config->MQTT_TOPIC_NAME + MQTT_TOPIC_PUB_LWT;

  if(config->CAN_ENABLED) {
    subscribeCANTopic   = config->MQTT_TOPIC_NAME + "SET/" + config->CAN_CONFIG->CAN_MQTT_TOPIC_NAME;
    canTopicLength      = subscribeCANTopic.length();

    if(config->MQTT_USE_ONETOPIC) {
      publishCANTopic = config->MQTT_TOPIC_NAME + config->MQTT_ONETOPIC_NAME + config->CAN_CONFIG->CAN_MQTT_TOPIC_NAME;
    } else {
      publishCANTopic = config->MQTT_TOPIC_NAME + config->CAN_CONFIG->CAN_MQTT_TOPIC_NAME;
    }
  }

  reconnectMqtt();
}

void reconnectMqtt()
{
  //in case loopback as server is set, skip connecting (debug purpose)
  if(blockReconnects) {
    return;
  }

  if(reconnectionInProgress) {
    if(millis() - reconnectStartMillis >= 5000) {
      reconnectionInProgress = false;
      if (reconnectionAtempts++ == 100) {
        debugSerial.printf("Tried for 500 sec, rebooting now.");
        restart_board();
      }
    }
    return;
  }

  // debug wifi connection info
  String id = "Attempting MQTT connection with ID: ESPAltherma-";
  id += WiFi.macAddress().substring(6);
  debugSerial.println(id);

  // try to connect
  if (client.connect(id.c_str(), config->MQTT_USERNAME.c_str(), config->MQTT_PASSWORD.c_str(), publishLWTTopic.c_str(), 0, true, "Offline")) {
    reconnectionInProgress = false;
    reconnectionAtempts = 0;
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
    reconnectionInProgress = true;
    reconnectStartMillis = millis();
  }
}

MQTTSubscribeTopic getTopic(const char* topic)
{
  if (config->HEATING_ENABLED && subscribeHeatingTopic == topic) {
    return MQTTSubscribeTopic::Heating;
  } else if (config->COOLING_ENABLED && subscribeCoolingTopic == topic) {
    return MQTTSubscribeTopic::Cooling;
  } else if (subscribePowerTopic == topic) {
    return MQTTSubscribeTopic::Power;
  } else if (config->SG_ENABLED && subscribeSGTopic == topic) {
    return MQTTSubscribeTopic::SmartGrid;
  } else if (config->CAN_ENABLED && String(topic).startsWith(subscribeCANTopic)) {
    return MQTTSubscribeTopic::CAN;
  } else if (config->SAFETY_ENABLED && subscribeSafetyTopic == topic) {
    return MQTTSubscribeTopic::Safety;
  }

  return MQTTSubscribeTopic(0);
}

void mqttPublish(const MQTTPublishTopic topic, const char* payload)
{
  mqttPublish(topic, payload, nullptr, false);
}

void mqttPublish(const MQTTPublishTopic topic, const char* payload, const boolean retained)
{
  mqttPublish(topic, payload, nullptr, retained);
}

void mqttPublish(const MQTTPublishTopic topic, const char* payload, const char* label, const boolean retained)
{
  if (config->HEATING_ENABLED && MQTTPublishTopic::Heating == topic) {
    client.publish(publishHeatingTopic.c_str(), payload, retained);
  } else if (config->COOLING_ENABLED && MQTTPublishTopic::Cooling == topic) {
    client.publish(publishCoolingTopic.c_str(), payload, retained);
  } else if (MQTTPublishTopic::Power == topic) {
    client.publish(publishPowerTopic.c_str(), payload, retained);
  } else if (config->SG_ENABLED && MQTTPublishTopic::SmartGrid == topic) {
    client.publish(publishSGTopic.c_str(), payload, retained);
  } else if (config->SAFETY_ENABLED && MQTTPublishTopic::Safety == topic) {
    client.publish(publishSafetyTopic.c_str(), payload, retained);
  } else if (config->CAN_ENABLED && MQTTPublishTopic::CAN == topic) {
    client.publish((publishCANTopic + label).c_str(), payload);
  }
}

int mqttCANTopicLength()
{
  return canTopicLength;
}

bool mqttConnected()
{
  return client.connected();
}

void mqttLoop()
{
  client.loop();
}

void mqttSetCallback(MQTT_CALLBACK_SIGNATURE)
{
  client.setCallback(callback);
}