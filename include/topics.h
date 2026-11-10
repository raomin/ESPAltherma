#ifndef ESPALTHERMA_TOPICS_H
#define ESPALTHERMA_TOPICS_H

// MQTT topics and Home Assistant identifiers, all derived from the configurable base topic ("espaltherma" by
// default). Two boards on one broker, say one on the indoor unit and one on the outdoor unit of an Altherma HT,
// need different bases: every topic and every Home Assistant entity then starts with it.
// With the default base every string is exactly what earlier versions published, so nothing moves for the
// existing installations. No Arduino dependency: tested natively.

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#define TOPICS_DEFAULT_BASE "espaltherma"
// Sizes from the longest base topic (32 characters): nothing may be cut, a cut topic is another topic (two of the
// power limit's became the same one) and a cut identifier another entity
#define TOPICS_BASE_MAX 32
#define TOPIC_SIZE (TOPICS_BASE_MAX + 32)     // base + "/power_limit/attributes" (23)
#define TOPIC_ID_SIZE (TOPICS_BASE_MAX + 40)  // "button." + id + "_leaving_water_curve_restore" (35), the device name (34)
#define TOPIC_CFG_SIZE (TOPICS_BASE_MAX + 64) // "homeassistant/button/" + id + "/lwt_target_restore/config" (47)
#define TOPICS_DEFAULT_CLIENT_ID "ESPAltherma-dev"

// Cleans a base topic typed by the user: no spaces, no MQTT wildcards, no control characters, no leading or
// trailing slash; empty becomes the default. Returns true when something was changed.
inline bool topicsSanitizeBase(char *base, size_t size)
{
  char out[64];
  size_t n = 0;
  for (const char *p = base; *p && n < sizeof(out) - 1; p++)
  {
    unsigned char c = (unsigned char)*p;
    if (c <= 0x20 || c >= 0x7F || c == '#' || c == '+')
      continue;
    out[n++] = (char)c;
  }
  out[n] = 0;
  while (n > 0 && out[n - 1] == '/')
    out[--n] = 0;
  const char *s = out;
  while (*s == '/')
    s++;
  if (*s == 0)
    s = TOPICS_DEFAULT_BASE;
  bool changed = strcmp(base, s) != 0;
  strncpy(base, s, size - 1);
  base[size - 1] = 0;
  return changed;
}

struct MqttTopics
{
  char base[TOPICS_BASE_MAX + 1];           // the base topic as configured
  char id[TOPICS_BASE_MAX + 1];             // the base as an identifier: lower case, letters, digits and underscores
  bool legacy;             // the default base: the exact strings of earlier versions
  // topics
  char attr[TOPIC_SIZE], lwt[TOPIC_SIZE], log[TOPIC_SIZE], detect[TOPIC_SIZE], detectRun[TOPIC_SIZE], power[TOPIC_SIZE], state[TOPIC_SIZE];
  char sgSet[TOPIC_SIZE], sgState[TOPIC_SIZE], safety[TOPIC_SIZE], safetyState[TOPIC_SIZE], serialTx[TOPIC_SIZE], serialRx[TOPIC_SIZE];
  char ctlSet[2][TOPIC_SIZE], ctlState[2][TOPIC_SIZE], ctlAttrs[2][TOPIC_SIZE];
  char limitSet[TOPIC_SIZE], limitState[TOPIC_SIZE], limitAttrs[TOPIC_SIZE], limitOnSet[TOPIC_SIZE], limitOnState[TOPIC_SIZE]; // the power limit
  // Home Assistant discovery topics and identifiers
  char cfgSensor[TOPIC_CFG_SIZE], cfgSwitch[TOPIC_CFG_SIZE], cfgSg[TOPIC_CFG_SIZE], cfgSafety[TOPIC_CFG_SIZE], cfgCtl[2][TOPIC_CFG_SIZE], cfgRestore[2][TOPIC_CFG_SIZE], cfgDevice[TOPIC_CFG_SIZE];
  char cfgLimit[TOPIC_CFG_SIZE], cfgLimitOn[TOPIC_CFG_SIZE];
  char deviceId[TOPIC_SIZE], deviceName[TOPIC_ID_SIZE];   // the device of the readings (device-based discovery)
  char relayDeviceId[TOPIC_SIZE], relayDeviceName[TOPIC_SIZE]; // the device of the relays (the legacy entities)
  char ctlUniq[2][TOPIC_SIZE], ctlEntity[2][TOPIC_ID_SIZE];
  char restoreUniq[2][TOPIC_SIZE], restoreEntity[2][TOPIC_ID_SIZE]; // the "restore the unit's curve" buttons
  char limitUniq[TOPIC_SIZE], limitEntity[TOPIC_ID_SIZE], limitOnUniq[TOPIC_SIZE], limitOnEntity[TOPIC_ID_SIZE];
  char updateTopic[TOPIC_SIZE], cfgUpdate[TOPIC_CFG_SIZE], updateUniq[TOPIC_SIZE], updateEntity[TOPIC_ID_SIZE]; // the firmware update entity
  char clientId[TOPIC_SIZE];       // the MQTT client id: the broker drops a connection when another one takes its id

  void init(const char *baseTopic, const char *configuredClientId = TOPICS_DEFAULT_CLIENT_ID)
  {
    strncpy(base, baseTopic && *baseTopic ? baseTopic : TOPICS_DEFAULT_BASE, sizeof(base) - 1);
    base[sizeof(base) - 1] = 0;
    // identifier: lower case, anything else becomes an underscore, none at the ends
    size_t n = 0;
    for (const char *p = base; *p && n < sizeof(id) - 1; p++)
    {
      unsigned char c = (unsigned char)*p;
      char d = isalnum(c) ? (char)tolower(c) : '_';
      if (d == '_' && (n == 0 || id[n - 1] == '_'))
        continue;
      id[n++] = d;
    }
    while (n > 0 && id[n - 1] == '_')
      n--;
    id[n] = 0;
    if (n == 0)
      strcpy(id, TOPICS_DEFAULT_BASE);
    legacy = strcmp(base, TOPICS_DEFAULT_BASE) == 0;

    snprintf(attr, sizeof(attr), "%s/ATTR", base);
    snprintf(lwt, sizeof(lwt), "%s/LWT", base);
    snprintf(log, sizeof(log), "%s/log", base);
    snprintf(detect, sizeof(detect), "%s/detect", base);
    snprintf(detectRun, sizeof(detectRun), "%s/detect/run", base);
    snprintf(power, sizeof(power), "%s/POWER", base);
    snprintf(state, sizeof(state), "%s/STATE", base);
    snprintf(sgSet, sizeof(sgSet), "%s/sg/set", base);
    snprintf(sgState, sizeof(sgState), "%s/sg/state", base);
    snprintf(safety, sizeof(safety), "%s/SAFETY", base);
    snprintf(safetyState, sizeof(safetyState), "%s/SAFETY_STATE", base);
    snprintf(serialTx, sizeof(serialTx), "%s/serialTX", base);
    snprintf(serialRx, sizeof(serialRx), "%s/serialRX", base);
    const char *ctl[2] = {"dhw_target", "lwt_target"};
    const char *entity[2] = {"hot_water_target", "leaving_water_target"};
    for (int k = 0; k < 2; k++)
    {
      snprintf(ctlSet[k], sizeof(ctlSet[k]), "%s/%s/set", base, ctl[k]);
      snprintf(ctlState[k], sizeof(ctlState[k]), "%s/%s/state", base, ctl[k]);
      snprintf(ctlAttrs[k], sizeof(ctlAttrs[k]), "%s/%s/attributes", base, ctl[k]);
      snprintf(ctlUniq[k], sizeof(ctlUniq[k]), "%s_%s", id, ctl[k]);
      snprintf(ctlEntity[k], sizeof(ctlEntity[k]), "number.%s_%s", id, entity[k]);
      snprintf(restoreUniq[k], sizeof(restoreUniq[k]), "%s_%s_restore", id, ctl[k]);
      snprintf(restoreEntity[k], sizeof(restoreEntity[k]), "button.%s_%s_curve_restore", id, k == 0 ? "hot_water" : "leaving_water");
    }
    snprintf(limitSet, sizeof(limitSet), "%s/power_limit/set", base);
    snprintf(limitState, sizeof(limitState), "%s/power_limit/state", base);
    snprintf(limitAttrs, sizeof(limitAttrs), "%s/power_limit/attributes", base);
    snprintf(limitOnSet, sizeof(limitOnSet), "%s/power_limit/on/set", base);
    snprintf(limitOnState, sizeof(limitOnState), "%s/power_limit/on/state", base);
    snprintf(limitUniq, sizeof(limitUniq), "%s_power_limit", id);
    snprintf(limitEntity, sizeof(limitEntity), "number.%s_power_limit", id);
    snprintf(limitOnUniq, sizeof(limitOnUniq), "%s_power_limit_on", id);
    snprintf(limitOnEntity, sizeof(limitOnEntity), "switch.%s_power_limit", id);
    snprintf(updateTopic, sizeof(updateTopic), "%s/update", base);
    snprintf(updateUniq, sizeof(updateUniq), "%s_firmware", id);
    snprintf(updateEntity, sizeof(updateEntity), "update.%s_firmware", id);
    // discovery: the legacy strings had their own spellings, kept for the default base
    const char *seg = legacy ? "espAltherma" : id;
    const char *dev = legacy ? "espaltherma" : id;
    snprintf(cfgSensor, sizeof(cfgSensor), "homeassistant/sensor/%s/config", seg);
    snprintf(cfgSwitch, sizeof(cfgSwitch), "homeassistant/switch/%s/config", seg);
    snprintf(cfgSg, sizeof(cfgSg), "homeassistant/select/%s/sg/config", seg);
    snprintf(cfgSafety, sizeof(cfgSafety), "homeassistant/switch/%s/safety/config", seg);
    for (int k = 0; k < 2; k++)
    {
      snprintf(cfgCtl[k], sizeof(cfgCtl[k]), "homeassistant/number/%s/%s/config", seg, ctl[k]);
      snprintf(cfgRestore[k], sizeof(cfgRestore[k]), "homeassistant/button/%s/%s_restore/config", seg, ctl[k]);
    }
    snprintf(cfgLimit, sizeof(cfgLimit), "homeassistant/number/%s/power_limit/config", seg);
    snprintf(cfgLimitOn, sizeof(cfgLimitOn), "homeassistant/switch/%s/power_limit/config", seg);
    snprintf(cfgUpdate, sizeof(cfgUpdate), "homeassistant/update/%s/firmware/config", seg);
    snprintf(cfgDevice, sizeof(cfgDevice), "homeassistant/device/%s-mqtt-discovery/config", dev);
    snprintf(deviceId, sizeof(deviceId), "%s-mqtt-discovery", dev);
    // the default client id would be shared by both boards: it follows the base, a chosen one is kept
    if (legacy || !configuredClientId || (*configuredClientId && strcmp(configuredClientId, TOPICS_DEFAULT_CLIENT_ID) != 0))
      snprintf(clientId, sizeof(clientId), "%s", configuredClientId && *configuredClientId ? configuredClientId : TOPICS_DEFAULT_CLIENT_ID);
    else
      snprintf(clientId, sizeof(clientId), "ESPAltherma-%s", id);
    if (legacy)
    {
      strcpy(deviceName, "Daikin Altherma via ESPAltherma");
      strcpy(relayDeviceId, "ESPAltherma");
      strcpy(relayDeviceName, "ESPAltherma");
    }
    else
    {
      snprintf(deviceName, sizeof(deviceName), "Daikin Altherma via ESPAltherma (%s)", base);
      snprintf(relayDeviceId, sizeof(relayDeviceId), "ESPAltherma-%s", id);
      snprintf(relayDeviceName, sizeof(relayDeviceName), "ESPAltherma (%s)", base);
    }
  }
};

#endif
