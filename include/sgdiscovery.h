#ifndef ESPALTHERMA_SGDISCOVERY_H
#define ESPALTHERMA_SGDISCOVERY_H
#include <stdio.h>
#include "topics.h"

// The "AlthermaSensors" entity, which every board publishes at each connection: it now carries the device of the
// board in full, its name, "ESPAltherma" as the maker and the board as the model. The board's name was only in the
// Smart Grid select below, which a board without Smart Grid relays never sends: Home Assistant then kept the model
// it had stored from 1.x, "M5StickC PLUS ESP32-PICO" on every board, or showed a device with no name at all.
inline int boardSensorDiscovery(char *json,size_t size,const MqttTopics &topics,const char *board)
{
  return snprintf(json,size,
    "{\"name\":\"AlthermaSensors\",\"stat_t\":\"~/LWT\",\"avty_t\":\"~/LWT\",\"pl_avail\":\"Online\",\"pl_not_avail\":\"Offline\","
    "\"uniq_id\":\"%s\",\"device\":{\"identifiers\":[\"%s\"],\"manufacturer\":\"ESPAltherma\",\"model\":\"%s\",\"name\":\"%s\"}, \"~\":\"%s\",\"json_attr_t\":\"~/ATTR\"}",
    topics.id,topics.relayDeviceId,board,topics.relayDeviceName,topics.base);
}

inline int smartGridDiscovery(char *json,size_t size,const MqttTopics &topics,const char *board)
{
  return snprintf(json,size,
    "{\"availability\":[{\"topic\":\"%s\",\"payload_available\":\"Online\",\"payload_not_available\":\"Offline\"}],\"availability_mode\":\"all\","
    "\"unique_id\":\"%s_sg\",\"device\":{\"identifiers\":[\"%s\"],\"manufacturer\":\"ESPAltherma\",\"model\":\"%s\",\"name\":\"%s\"},"
    "\"icon\":\"mdi:solar-power\",\"name\":\"EspAltherma Smart Grid\",\"command_topic\":\"%s\","
    "\"command_template\":\"{%% if value == 'Free Running' %%} 0 {%% elif value == 'Forced Off' %%} 1 {%% elif value == 'Recommended On' %%} 2 {%% elif value == 'Forced On' %%} 3 {%% else %%} 0 {%% endif %%}\","
    "\"options\":[\"Free Running\",\"Forced Off\",\"Recommended On\",\"Forced On\"],\"state_topic\":\"%s\","
    "\"value_template\":\"{%% set mapper = { '0':'Free Running', '1':'Forced Off', '2':'Recommended On', '3':'Forced On' } %%} {%% set word = mapper[value] %%} {{ word }}\"}",
    topics.lwt,topics.id,topics.relayDeviceId,board,topics.relayDeviceName,topics.sgSet,topics.sgState);
}
#endif
