#ifndef ESPALTHERMA_SGDISCOVERY_H
#define ESPALTHERMA_SGDISCOVERY_H
#include <stdio.h>
#include "topics.h"

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
