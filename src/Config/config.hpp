#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <canBus.hpp>
#include <x10a.hpp>

#ifdef ARDUINO_ARCH_ESP8266
#define FILE_READ       "r"
#define FILE_WRITE      "w"
#define FILE_APPEND     "a"
#endif

#define CONFIG_FILE "/config.json"
#define MODELS_CONFIG_SIZE 1024*10

struct ESPAlthermaConfig
{
    bool configStored;
    bool STANDALONE_WIFI;
    String SSID;
    String SSID_PASSWORD;
    bool SSID_STATIC_IP;
    String SSID_IP;
    String SSID_SUBNET;
    String SSID_GATEWAY;
    String SSID_PRIMARY_DNS;
    String SSID_SECONDARY_DNS;
    String MQTT_SERVER;
    String MQTT_USERNAME;
    String MQTT_PASSWORD;
    bool MQTT_USE_JSONTABLE;
    bool MQTT_USE_ONETOPIC;
    String MQTT_TOPIC_NAME;
    String MQTT_ONETOPIC_NAME;
    uint16_t MQTT_PORT;
    bool MQTT_DISABLE_LOG_MESSAGES;
    bool MQTT_ENCRYPTED;
    uint8_t PIN_ENABLE_CONFIG;
    bool X10A_ENABLED;
    X10A_Config* X10A_CONFIG;
    bool HEATING_ENABLED;
    uint8_t PIN_HEATING;
    bool COOLING_ENABLED;
    uint8_t PIN_COOLING;
    bool SG_ENABLED;
    uint8_t PIN_SG1;
    uint8_t PIN_SG2;
    bool SG_RELAY_HIGH_TRIGGER;
    bool CAN_ENABLED;
    bool SAFETY_ENABLED;
    uint8_t PIN_SAFETY;
    CAN_Config* CAN_CONFIG;

    ~ESPAlthermaConfig();
};

extern ESPAlthermaConfig* config;

void readConfig();

void fillX10AParameters(JsonObject &jsonObject, X10A_Config *X10AConfig);

void saveConfig();

#endif