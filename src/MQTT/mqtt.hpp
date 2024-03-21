#ifndef MQTT_H
#define MQTT_H

#include <PubSubClient.h>
#include <WiFiClientSecure.h>
#include "Config/config.hpp"
#include "persistence.hpp"
#include "restart.hpp"

#define MQTT_TOPIC_SUB_HEATING  "SET/HEATING"
#define MQTT_TOPIC_SUB_COOLING  "SET/COOLING"
#define MQTT_TOPIC_SUB_SAFETY   "SET/SAFETY"
#define MQTT_TOPIC_SUB_SG       "SET/SG"
#define MQTT_TOPIC_SUB_POWER    "SET/POWER"

#define MQTT_TOPIC_PUB_HEATING  "STATE/HEATING"
#define MQTT_TOPIC_PUB_COOLING  "STATE/COOLING"
#define MQTT_TOPIC_PUB_SAFETY   "STATE/SAFETY"
#define MQTT_TOPIC_PUB_SG       "STATE/SG"
#define MQTT_TOPIC_PUB_POWER    "STATE/POWER"
#define MQTT_TOPIC_PUB_ATTR     "ATTR"
#define MQTT_TOPIC_PUB_LWT      "LWT"

#define EEPROM_CHK 1
#define EEPROM_STATE 0

#define MAX_MSG_SIZE 7120 // max size of the json message sent in mqtt

extern char jsonbuff[MAX_MSG_SIZE];
extern uint8_t SG_RELAY_ACTIVE_STATE;
extern uint8_t SG_RELAY_INACTIVE_STATE;

enum class MQTTSubscribeTopic : uint8_t
{
  Heating = 1,
  Cooling,
  SmartGrid,
  CAN,
  Safety,
  Power
};

enum class MQTTPublishTopic : uint8_t
{
  Heating = 1,
  Cooling,
  SmartGrid,
  CAN,
  Safety,
  Power
};

void initMQTT();

void createEmptyJSONBuffer();

void updateValues(ParameterDef *labelDef);

void sendValues();

void connectMqtt();

void reconnectMqtt();

bool mqttConnected();

void mqttLoop();

MQTTSubscribeTopic getTopic(const char* topic);

void mqttPublish(const MQTTPublishTopic topic, const char* payload);
void mqttPublish(const MQTTPublishTopic topic, const char* payload, const boolean retained);
void mqttPublish(const MQTTPublishTopic topic, const char* payload, const char* label, const boolean retained);

int mqttCANTopicLength();

void mqttSetCallback(MQTT_CALLBACK_SIGNATURE);

#endif