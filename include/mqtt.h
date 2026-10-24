#include <PubSubClient.h>
#include <EEPROM.h>
#include "restart.h"
#include "comm.h"

#include "topics.h"

// Every topic and every Home Assistant id, from the configurable base topic (Settings > MQTT output)
MqttTopics mqttTopics;
IPAddress netIP(); // netmgr.h, included later

#define EEPROM_CHK 1
#define EEPROM_STATE 0

#ifndef MAX_MSG_SIZE
#define MAX_MSG_SIZE 7120//max size of the json message sent in mqtt
#endif

char jsonbuff[MAX_MSG_SIZE] = "{\0";

#include <WiFiClientSecure.h>
WiFiClient plainClient;
WiFiClientSecure secureClient; // MQTT over TLS, a runtime option
PubSubClient client;
Client *mqttNet = &plainClient; // the network client under PubSubClient

extern Converter converter;
extern CurveControl dhwControl, lwtControl;
extern LimitControl limitControl;
static bool limitDiscoveryOn = false;   // the power limit entities are in Home Assistant
static LimitKind limitDiscoveryKind = LIMIT_POWER;

// The two target controls on MQTT (experimental, control.h): the entity's name and icon; the topics and the ids
// come from mqttTopics (ctlSet, ctlState, ctlAttrs, cfgCtl, ctlUniq, ctlEntity)
struct ControlNames
{
  const char *name, *icon;
};
static const ControlNames CONTROL_NAMES[2] = {{"Hot water target", "mdi:water-boiler"}, {"Leaving water target", "mdi:heating-coil"}};
static const char *RESTORE_NAMES[2] = {"Restore hot water curve", "Restore leaving water curve"};
bool controlDiscoveryOn[2] = {false, false}; // the number entity is published (with these limits)
int controlDiscoveryMin[2] = {0, 0}, controlDiscoveryMax[2] = {0, 0};
static bool controlEnabled(const CurveControl &c) { return c.kind == CONTROL_LWT ? config.lwtTarget : config.dhwTarget; }

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

// Selects the (plain or TLS) network client and the broker, from the configuration, and derives every topic and
// Home Assistant id from the base topic.
void setupMqttClient()
{
  mqttTopics.init(config.mqttBase, config.mqttClientId);
  haIdPrefix = mqttTopics.id;
  haDeviceId = mqttTopics.deviceId;
  haDeviceName = mqttTopics.deviceName;
  haAttrTopic = mqttTopics.attr;
  mqttSerial.begin(&client, mqttTopics.log);
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
  snprintf(jsonbuff + strlen(jsonbuff),MAX_MSG_SIZE - strlen(jsonbuff) , "\"%s\":\"%s\",", "IP", netIP().toString().c_str());
  snprintf(jsonbuff + strlen(jsonbuff),MAX_MSG_SIZE - strlen(jsonbuff) , "\"%s\":\"%d\",", "FreeMem", ESP.getFreeHeap());
  snprintf(jsonbuff + strlen(jsonbuff),MAX_MSG_SIZE - strlen(jsonbuff) , "\"%s\":\"%lu\",", "Uptime", (unsigned long)(millis() / 1000)); // seconds
  jsonbuff[strlen(jsonbuff) - 1] = '}';
  if (config.jsonTable)
  {
    strcat(jsonbuff,"]");
  }
  mqttPublish(mqttTopics.attr, jsonbuff);
  resetJson();
}

// Removes every entity this base topic published from Home Assistant, and marks it offline: called before the
// base topic changes, so the old names do not linger as dead entities. Main loop only.
void mqttRetireDiscovery()
{
  if (!client.connected())
    return;
  const char *cfgs[] = {mqttTopics.cfgSensor, mqttTopics.cfgSwitch, mqttTopics.cfgSg, mqttTopics.cfgSafety,
                        mqttTopics.cfgCtl[0], mqttTopics.cfgCtl[1], mqttTopics.cfgRestore[0], mqttTopics.cfgRestore[1],
                        mqttTopics.cfgLimit, mqttTopics.cfgLimitOn, mqttTopics.cfgDevice};
  for (const char *t : cfgs)
    client.publish(t, "", true);
  const char *states[] = {mqttTopics.ctlState[0], mqttTopics.ctlState[1], mqttTopics.ctlAttrs[0], mqttTopics.ctlAttrs[1],
                          mqttTopics.state, mqttTopics.safetyState, mqttTopics.detect};
  for (const char *t : states)
    client.publish(t, "", true);
  client.publish(mqttTopics.lwt, "Offline", true);
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

// One connection attempt; on success publishes the discovery messages and subscribes.
// The Smart Grid relays keep their state through a broker reconnect or a Home Assistant restart, so the
// reported mode is the real one, retained: Home Assistant shows it again after its own restart.
void publishSgState()
{
  char s[2] = {(char)('0' + sgMode), 0};
  client.publish(mqttTopics.sgState, s, true);
}

bool mqttConnectOnce()
{
    mqttSerial.print("Attempting MQTT connection...");

    if (client.connect(mqttTopics.clientId, config.mqttUser, config.mqttPwd, mqttTopics.lwt, 0, true, "Offline"))
    {
      mqttSerial.println("connected!");
      static char json[1100]; // the discovery payloads, built from the base topic
      snprintf(json, sizeof(json),
               "{\"name\":\"AlthermaSensors\",\"stat_t\":\"~/LWT\",\"avty_t\":\"~/LWT\",\"pl_avail\":\"Online\",\"pl_not_avail\":\"Offline\","
               "\"uniq_id\":\"%s\",\"device\":{\"identifiers\":[\"%s\"]}, \"~\":\"%s\",\"json_attr_t\":\"~/ATTR\"}",
               mqttTopics.id, mqttTopics.relayDeviceId, mqttTopics.base);
      client.publish(mqttTopics.cfgSensor, json, true);
      client.publish(mqttTopics.lwt, "Online", true);
      if (config.thermPin >= 0)
      {
        if (mqttTopics.legacy) // the legacy switch had no unique id: keep it so for the default base
          snprintf(json, sizeof(json), "{\"name\":\"Altherma\",\"cmd_t\":\"~/POWER\",\"stat_t\":\"~/STATE\",\"pl_off\":\"OFF\",\"pl_on\":\"ON\",\"~\":\"%s\"}",
                   mqttTopics.base);
        else // a second board needs its own entity
          snprintf(json, sizeof(json), "{\"name\":\"Altherma\",\"cmd_t\":\"~/POWER\",\"stat_t\":\"~/STATE\",\"pl_off\":\"OFF\",\"pl_on\":\"ON\",\"~\":\"%s\","
                   "\"uniq_id\":\"%s_thermostat\",\"device\":{\"identifiers\":[\"%s\"],\"name\":\"%s\"}}",
                   mqttTopics.base, mqttTopics.id, mqttTopics.relayDeviceId, mqttTopics.relayDeviceName);
        client.publish(mqttTopics.cfgSwitch, json, true);
      }
      else
      {
        // No thermostat relay: remove the switch from HA
        client.publish(mqttTopics.cfgSwitch, "", true);
      }

      publishHomeAssistantDeviceDiscovery();

      // Subscribe
      client.subscribe(mqttTopics.power);
      client.subscribe(mqttTopics.detectRun);
      if (config.sg1Pin >= 0)
      {
      // Smart Grid
      snprintf(json, sizeof(json),
               "{\"availability\":[{\"topic\":\"%s\",\"payload_available\":\"Online\",\"payload_not_available\":\"Offline\"}],\"availability_mode\":\"all\","
               "\"unique_id\":\"%s_sg\",\"device\":{\"identifiers\":[\"%s\"],\"manufacturer\":\"ESPAltherma\",\"model\":\"M5StickC PLUS ESP32-PICO\",\"name\":\"%s\"},"
               "\"icon\":\"mdi:solar-power\",\"name\":\"EspAltherma Smart Grid\",\"command_topic\":\"%s\","
               "\"command_template\":\"{%% if value == 'Free Running' %%} 0 {%% elif value == 'Forced Off' %%} 1 {%% elif value == 'Recommended On' %%} 2 {%% elif value == 'Forced On' %%} 3 {%% else %%} 0 {%% endif %%}\","
               "\"options\":[\"Free Running\",\"Forced Off\",\"Recommended On\",\"Forced On\"],\"state_topic\":\"%s\","
               "\"value_template\":\"{%% set mapper = { '0':'Free Running', '1':'Forced Off', '2':'Recommended On', '3':'Forced On' } %%} {%% set word = mapper[value] %%} {{ word }}\"}",
               mqttTopics.lwt, mqttTopics.id, mqttTopics.relayDeviceId, mqttTopics.relayDeviceName, mqttTopics.sgSet, mqttTopics.sgState);
      client.publish(mqttTopics.cfgSg, json, true);
      client.subscribe(mqttTopics.sgSet);
      publishSgState();
      }
      else
      {
      // Publish empty retained message so discovered entities are removed from HA
      client.publish(mqttTopics.cfgSg, "", true);
      }

      if (config.safetyPin >= 0)
      {
      // Safety relay
      if (mqttTopics.legacy)
        snprintf(json, sizeof(json), "{\"name\":\"Altherma Safety\",\"cmd_t\":\"~/SAFETY\",\"stat_t\":\"~/SAFETY_STATE\",\"pl_off\":\"0\",\"pl_on\":\"1\",\"~\":\"%s\"}",
                 mqttTopics.base);
      else
        snprintf(json, sizeof(json), "{\"name\":\"Altherma Safety\",\"cmd_t\":\"~/SAFETY\",\"stat_t\":\"~/SAFETY_STATE\",\"pl_off\":\"0\",\"pl_on\":\"1\",\"~\":\"%s\","
                 "\"uniq_id\":\"%s_safety\",\"device\":{\"identifiers\":[\"%s\"],\"name\":\"%s\"}}",
                 mqttTopics.base, mqttTopics.id, mqttTopics.relayDeviceId, mqttTopics.relayDeviceName);
      client.publish(mqttTopics.cfgSafety, json, true);
      client.subscribe(mqttTopics.safety);
      }

      if (config.debugSerial)
      {
      // DebugSerial - MQTT<>Serial gateway
      client.subscribe(mqttTopics.serialTx);
      }

      // Target controls (experimental): the number entities follow the controls' state, published by the main loop
      for (CurveControl *c : {&dhwControl, &lwtControl})
      {
        controlDiscoveryOn[c->kind] = false;
        c->changed = true;
        if (controlEnabled(*c))
          client.subscribe(mqttTopics.ctlSet[c->kind]);
        else
        {
          client.publish(mqttTopics.cfgCtl[c->kind], "", true); // option off: remove the entities from HA
          client.publish(mqttTopics.cfgRestore[c->kind], "", true);
        }
      }
      limitDiscoveryOn = false;
      limitControl.changed = true;
      if (config.powerLimit)
      {
        client.subscribe(mqttTopics.limitSet);
        client.subscribe(mqttTopics.limitOnSet);
      }
      else
      {
        client.publish(mqttTopics.cfgLimit, "", true);
        client.publish(mqttTopics.cfgLimitOn, "", true);
      }
      return true;
    }
    mqttSerial.printf("failed, rc=%d, try again in 5 seconds", client.state());
    return false;
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
    client.publish(mqttTopics.state, "OFF", true);
    mqttSerial.println("Turned OFF");
  }
  else if (payload[1] == 'N')
  { //turn on
    digitalWrite(config.thermPin, thermActiveState());
    saveEEPROM(thermActiveState());
    thermostatOn = true;
    client.publish(mqttTopics.state, "ON", true);
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
    publishSgState();
    mqttSerial.println("Set SG mode to 0 - Normal operation");
  }
  else if (payload[0] == '1')
  {
    // Set SG 1 mode => SG1 = INACTIVE, SG2 = ACTIVE
    digitalWrite(config.sg1Pin, sgInactiveState());
    digitalWrite(config.sg2Pin, sgActiveState());
    sgMode = 1;
    publishSgState();
    mqttSerial.println("Set SG mode to 1 - Forced OFF");
  }
  else if (payload[0] == '2')
  {
    // Set SG 2 mode => SG1 = ACTIVE, SG2 = INACTIVE
    digitalWrite(config.sg1Pin, sgActiveState());
    digitalWrite(config.sg2Pin, sgInactiveState());
    sgMode = 2;
    publishSgState();
    mqttSerial.println("Set SG mode to 2 - Recommended ON");
  }
  else if (payload[0] == '3')
  {
    // Set SG 3 mode => SG1 = ACTIVE, SG2 = ACTIVE
    digitalWrite(config.sg1Pin, sgActiveState());
    digitalWrite(config.sg2Pin, sgActiveState());
    sgMode = 3;
    publishSgState();
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
    client.publish(mqttTopics.safetyState, "0", true);
  }
  else if (payload[0] == '1')
  {
    // Set Safety relay to ON
    digitalWrite(config.safetyPin, safetyActiveState());
    safetyActive = true;
    client.publish(mqttTopics.safetyState, "1", true);
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
    
    client.publish(mqttTopics.serialRx, hexResponse.c_str());
  }
}


// The power limit requested over MQTT: a value in kW or A (the unit's kind), or ON/OFF on the switch topic.
void callbackLimit(byte *payload, unsigned int length, bool onOff)
{
  payload[length] = '\0';
  if (limitControl.status != CONTROL_READY)
  {
    mqttSerial.printf("Power limit: not available (%s)\n", controlStatusName(limitControl.status));
    return;
  }
  if (onOff)
  {
    if (strcasecmp((const char *)payload, "ON") == 0)
      limitControl.requestOn(true);
    else if (strcasecmp((const char *)payload, "OFF") == 0)
      limitControl.requestOn(false);
    else
      mqttSerial.printf("Power limit: ignored '%s'\n", payload);
    return;
  }
  char *end = nullptr;
  float value = strtof((const char *)payload, &end);
  if (end == (const char *)payload || value < limitControl.minLimit || value > limitControl.maxLimit)
  {
    mqttSerial.printf("Power limit: ignored '%s'\n", payload);
    return;
  }
  limitControl.request(value);
}

// A target requested over MQTT: a whole number of degrees, applied by the heat pump task.
void callbackControl(CurveControl &c, byte *payload, unsigned int length)
{
  const char *name = CONTROL_NAMES[c.kind].name;
  payload[length] = '\0';
  if (strcasecmp((const char *)payload, "restore") == 0)
  {
    if (c.status != CONTROL_READY)
      mqttSerial.printf("%s: not available (%s)\n", name, controlStatusName(c.status));
    else if (c.origA < 0)
      mqttSerial.printf("%s: no curve to restore\n", name);
    else
      c.requestRestore();
    return;
  }
  char *end = nullptr;
  long value = strtol((const char *)payload, &end, 10);
  if (end == (const char *)payload || value < 5 || value > 90)
  {
    mqttSerial.printf("%s: ignored '%s'\n", name, payload);
    return;
  }
  if (c.status != CONTROL_READY)
  {
    mqttSerial.printf("%s: not available (%s)\n", name, controlStatusName(c.status));
    return;
  }
  c.request((int)value);
}

// The number entity, its state and its attributes: created when the control is ready, removed when it is not.
// Main loop only.
void publishControl(CurveControl &c)
{
  int k = c.kind;
  bool ready = controlEnabled(c) && c.status == CONTROL_READY;
  bool &on = controlDiscoveryOn[c.kind];
  if (ready && (!on || controlDiscoveryMin[c.kind] != c.minTarget || controlDiscoveryMax[c.kind] != c.maxTarget))
  {
    // Same device as the readings (homeassistant.cpp): its identifier and its name, which Home Assistant
    // requires in every entity that shares a device. def_ent_id fixes the entity id, like the readings do.
    char json[640];
    snprintf(json, sizeof(json),
             "{\"name\":\"%s\",\"uniq_id\":\"%s\",\"def_ent_id\":\"%s\",\"cmd_t\":\"%s\",\"stat_t\":\"%s\",\"json_attr_t\":\"%s\","
             "\"avty_t\":\"%s\",\"pl_avail\":\"Online\",\"pl_not_avail\":\"Offline\","
             "\"min\":%d,\"max\":%d,\"step\":1,\"unit_of_meas\":\"°C\",\"dev_cla\":\"temperature\",\"mode\":\"box\","
             "\"icon\":\"%s\",\"dev\":{\"ids\":\"%s\",\"name\":\"%s\"}}",
             CONTROL_NAMES[k].name, mqttTopics.ctlUniq[k], mqttTopics.ctlEntity[k], mqttTopics.ctlSet[k], mqttTopics.ctlState[k], mqttTopics.ctlAttrs[k],
             mqttTopics.lwt, c.minTarget, c.maxTarget, CONTROL_NAMES[k].icon, mqttTopics.deviceId, mqttTopics.deviceName);
    mqttPublish(mqttTopics.cfgCtl[k], json, true);
    on = true;
    controlDiscoveryMin[c.kind] = c.minTarget;
    controlDiscoveryMax[c.kind] = c.maxTarget;
  }
  else if (!ready && on)
  {
    mqttPublish(mqttTopics.cfgCtl[k], "", true); // removes the entity
    on = false;
  }
  if (ready && c.target >= 0)
  {
    char value[8], attrs[160];
    snprintf(value, sizeof(value), "%d", c.target);
    mqttPublish(mqttTopics.ctlState[k], value, true);
    // What the thermostat really aims for, the unit's own maximum, and the second point of the curve
    snprintf(attrs, sizeof(attrs), "{\"effective\":%d,\"ceiling\":%d,\"curve_low\":%d,\"saved\":%d,\"min\":%d,\"max\":%d,\"curve_orig_hi\":%d,\"curve_orig_lo\":%d,\"restoring\":%s}",
             c.effective, c.ceiling, c.targetLo, c.saved, c.minTarget, c.maxTarget, c.origA, c.origB, c.restoring ? "true" : "false");
    mqttPublish(mqttTopics.ctlAttrs[k], attrs, true);
  }
  // The "restore the unit's curve" button: present while a curve from before the first write is known
  static bool restoreOn[2] = {false, false};
  bool restorable = ready && c.origA >= 0;
  if (restorable && !restoreOn[k])
  {
    char json[480];
    snprintf(json, sizeof(json),
             "{\"name\":\"%s\",\"uniq_id\":\"%s\",\"def_ent_id\":\"%s\",\"cmd_t\":\"%s\",\"pl_prs\":\"restore\","
             "\"avty_t\":\"%s\",\"pl_avail\":\"Online\",\"pl_not_avail\":\"Offline\",\"icon\":\"mdi:backup-restore\","
             "\"dev\":{\"ids\":\"%s\",\"name\":\"%s\"}}",
             RESTORE_NAMES[k], mqttTopics.restoreUniq[k], mqttTopics.restoreEntity[k], mqttTopics.ctlSet[k],
             mqttTopics.lwt, mqttTopics.deviceId, mqttTopics.deviceName);
    mqttPublish(mqttTopics.cfgRestore[k], json, true);
    restoreOn[k] = true;
  }
  else if (!restorable && restoreOn[k])
  {
    mqttPublish(mqttTopics.cfgRestore[k], "", true);
    restoreOn[k] = false;
  }
}

// The power limit: a number entity (kW or A, the unit's kind) and a switch, created when the control is ready,
// removed when it is not. Main loop only.
void publishLimit()
{
  LimitControl &c = limitControl;
  bool ready = config.powerLimit && c.status == CONTROL_READY;
  const char *unit = c.kind == LIMIT_POWER ? "kW" : "A";
  if (ready && (!limitDiscoveryOn || limitDiscoveryKind != c.kind))
  {
    char json[640];
    snprintf(json, sizeof(json),
             "{\"name\":\"Power limit\",\"uniq_id\":\"%s\",\"def_ent_id\":\"%s\",\"cmd_t\":\"%s\",\"stat_t\":\"%s\",\"json_attr_t\":\"%s\","
             "\"avty_t\":\"%s\",\"pl_avail\":\"Online\",\"pl_not_avail\":\"Offline\","
             "\"min\":%.1f,\"max\":%.1f,\"step\":%s,\"unit_of_meas\":\"%s\",\"mode\":\"box\",\"icon\":\"mdi:speedometer-slow\","
             "\"dev\":{\"ids\":\"%s\",\"name\":\"%s\"}}",
             mqttTopics.limitUniq, mqttTopics.limitEntity, mqttTopics.limitSet, mqttTopics.limitState, mqttTopics.limitAttrs,
             mqttTopics.lwt, c.minLimit, c.maxLimit, c.kind == LIMIT_POWER ? "0.5" : "1", unit, mqttTopics.deviceId, mqttTopics.deviceName);
    mqttPublish(mqttTopics.cfgLimit, json, true);
    snprintf(json, sizeof(json),
             "{\"name\":\"Power limit on\",\"uniq_id\":\"%s\",\"def_ent_id\":\"%s\",\"cmd_t\":\"%s\",\"stat_t\":\"%s\","
             "\"avty_t\":\"%s\",\"pl_avail\":\"Online\",\"pl_not_avail\":\"Offline\",\"icon\":\"mdi:speedometer-slow\","
             "\"dev\":{\"ids\":\"%s\",\"name\":\"%s\"}}",
             mqttTopics.limitOnUniq, mqttTopics.limitOnEntity, mqttTopics.limitOnSet, mqttTopics.limitOnState,
             mqttTopics.lwt, mqttTopics.deviceId, mqttTopics.deviceName);
    mqttPublish(mqttTopics.cfgLimitOn, json, true);
    limitDiscoveryOn = true;
    limitDiscoveryKind = c.kind;
  }
  else if (!ready && limitDiscoveryOn)
  {
    mqttPublish(mqttTopics.cfgLimit, "", true);
    mqttPublish(mqttTopics.cfgLimitOn, "", true);
    limitDiscoveryOn = false;
  }
  if (ready)
  {
    char value[16], attrs[160];
    float shown = c.on && c.value >= 0 ? c.value : c.limit;
    snprintf(value, sizeof(value), "%.1f", shown);
    mqttPublish(mqttTopics.limitState, value, true);
    mqttPublish(mqttTopics.limitOnState, c.on ? "ON" : "OFF", true);
    snprintf(attrs, sizeof(attrs), "{\"active\":%.1f,\"unit_limit\":%.1f,\"enable\":%d,\"kind\":\"%s\",\"min\":%.1f,\"max\":%.1f}",
             c.active, c.limit, c.enable, unit, c.minLimit, c.maxLimit);
    mqttPublish(mqttTopics.limitAttrs, attrs, true);
  }
}

void callback(char *topic, byte *payload, unsigned int length)
{
  mqttSerial.printf("Message arrived [%s] : %s\n", topic, payload);

  if (strcmp(topic, mqttTopics.power) == 0)
  {
    callbackTherm(payload, length);
  }
  else if (strcmp(topic, mqttTopics.detectRun) == 0)
  {
    mqttSerial.println("Heat pump survey requested");
    requestSurvey();
  }
  else if (config.sg1Pin >= 0 && strcmp(topic, mqttTopics.sgSet) == 0)
  {
    callbackSg(payload, length);
  }
  else if (config.safetyPin >= 0 && strcmp(topic, mqttTopics.safety) == 0)
  {
    callbackSafety(payload, length);
  }
  else if (config.debugSerial && strcmp(topic, mqttTopics.serialTx) == 0)
  {
    callbackDebugSerial(payload, length);
  }
  else if (config.dhwTarget && strcmp(topic, mqttTopics.ctlSet[CONTROL_DHW]) == 0)
  {
    callbackControl(dhwControl, payload, length);
  }
  else if (config.lwtTarget && strcmp(topic, mqttTopics.ctlSet[CONTROL_LWT]) == 0)
  {
    callbackControl(lwtControl, payload, length);
  }
  else if (config.powerLimit && strcmp(topic, mqttTopics.limitSet) == 0)
  {
    callbackLimit(payload, length, false);
  }
  else if (config.powerLimit && strcmp(topic, mqttTopics.limitOnSet) == 0)
  {
    callbackLimit(payload, length, true);
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

HaAnnounced haAnnounced; // the components of the last device discovery sent (haremoval.h)

void publishHomeAssistantDeviceDiscovery()
{
  valuesLock(); // the heat pump task may be changing the values
  const LabelDef *labels = converter.activeLabels;
  const size_t count = converter.activeCount;
  const char *topic = mqttTopics.cfgDevice;
  haBinaryAsNumber = config.binaryAsNumber; // binary sensors then read 1/0
  const std::vector<HaComponent> components = haComponents(labels, count);
  const std::vector<HaComponent> removed = haAnnounced.removed(components);

  // First pass: compute the exact payload length, required up-front for the MQTT fixed header.
  size_t payloadLen = streamDeviceDiscoveryPayload(labels, count, nullptr, nullptr, &removed);

  mqttSerial.printf("Sending HA discovery: %u sensors, %u removed, %u bytes\n",
                    (unsigned)(count + 5), (unsigned)removed.size(), (unsigned)payloadLen);

  // Stream the payload directly to the socket. PubSubClient::write() writes straight to the
  // TCP/TLS client, so the MQTT buffer only needs to hold the topic+header. This avoids the large
  // contiguous setBufferSize() allocation that previously ran out of heap on bigger definitions.
  if (!client.beginPublish(topic, payloadLen, true))
  {
    valuesUnlock();
    mqttSerial.println("Error starting device discovery publish!");
    return;
  }

  streamDeviceDiscoveryPayload(labels, count, mqttDiscoverySink, &client, &removed);

  if (client.endPublish() && !mqttWriteFailed)
    haAnnounced.sent(components); // the next discovery leaves the removed ones out
  valuesUnlock();
}
