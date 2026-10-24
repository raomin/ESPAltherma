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
  char base[33];           // the base topic as configured
  char id[33];             // the base as an identifier: lower case, letters, digits and underscores
  bool legacy;             // the default base: the exact strings of earlier versions
  // topics
  char attr[48], lwt[48], log[48], detect[48], detectRun[48], power[48], state[48];
  char sgSet[48], sgState[48], safety[48], safetyState[48], serialTx[48], serialRx[48];
  char ctlSet[2][48], ctlState[2][48], ctlAttrs[2][48];
  char limitSet[48], limitState[48], limitAttrs[48], limitOnSet[48], limitOnState[48]; // the power limit
  // Home Assistant discovery topics and identifiers
  char cfgSensor[80], cfgSwitch[80], cfgSg[80], cfgSafety[80], cfgCtl[2][80], cfgRestore[2][80], cfgDevice[80];
  char cfgLimit[80], cfgLimitOn[80];
  char deviceId[48], deviceName[64];   // the device of the readings (device-based discovery)
  char relayDeviceId[48], relayDeviceName[48]; // the device of the relays (the legacy entities)
  char ctlUniq[2][48], ctlEntity[2][64];
  char restoreUniq[2][48], restoreEntity[2][64]; // the "restore the unit's curve" buttons
  char limitUniq[48], limitEntity[64], limitOnUniq[48], limitOnEntity[64];
  char clientId[48];       // the MQTT client id: the broker drops a connection when another one takes its id

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
