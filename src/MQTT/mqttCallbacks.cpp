#include "mqttCallbacks.hpp"

std::function<void(const String &label, const char *payload, const uint32_t length)> callbackCAN;

void callbackHeating(byte *payload, unsigned int length)
{
  payload[length] = '\0';

  // is it ON or OFF?
  // ok I'm not super proud of this, but it works :p
  if (payload[1] == 'F') {
    // turn off
    digitalWrite(config->PIN_HEATING, HIGH);
    savePersistence();
    mqttPublish(MQTTPublishTopic::Heating, "OFF", true);
    debugSerial.println("Heating turned OFF");
  } else if (payload[1] == 'N') {
    // turn on
    digitalWrite(config->PIN_HEATING, LOW);
    savePersistence();
    mqttPublish(MQTTPublishTopic::Heating, "ON", true);
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
    mqttPublish(MQTTPublishTopic::Cooling, "OFF", true);
    debugSerial.println("Cooling turned OFF");
  } else if (payload[1] == 'N') {
    //turn on
    digitalWrite(config->PIN_COOLING, LOW);
    savePersistence();
    mqttPublish(MQTTPublishTopic::Cooling, "ON", true);
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
    mqttPublish(MQTTPublishTopic::SmartGrid, "0");
    debugSerial.println("Set SG mode to 0 - Normal operation");
  } else if (payload[0] == '1') {
    // set SG 1 mode => SG1 = INACTIVE, SG2 = ACTIVE
    digitalWrite(config->PIN_SG1, SG_RELAY_INACTIVE_STATE);
    digitalWrite(config->PIN_SG2, SG_RELAY_ACTIVE_STATE);
    mqttPublish(MQTTPublishTopic::SmartGrid, "1");
    debugSerial.println("Set SG mode to 1 - Forced OFF");
  } else if (payload[0] == '2') {
    // set SG 2 mode => SG1 = ACTIVE, SG2 = INACTIVE
    digitalWrite(config->PIN_SG1, SG_RELAY_ACTIVE_STATE);
    digitalWrite(config->PIN_SG2, SG_RELAY_INACTIVE_STATE);
    mqttPublish(MQTTPublishTopic::SmartGrid, "2");
    debugSerial.println("Set SG mode to 2 - Recommended ON");
  } else if (payload[0] == '3') {
    // set SG 3 mode => SG1 = ACTIVE, SG2 = ACTIVE
    digitalWrite(config->PIN_SG1, SG_RELAY_ACTIVE_STATE);
    digitalWrite(config->PIN_SG2, SG_RELAY_ACTIVE_STATE);
    mqttPublish(MQTTPublishTopic::SmartGrid, "3");
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
    mqttPublish(MQTTPublishTopic::Safety, "0", true);
  } else if (payload[0] == '1') {
    // Set Safety relay to ON
    digitalWrite(config->PIN_SAFETY, HIGH);
    mqttPublish(MQTTPublishTopic::Safety, "1", true);
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

  const MQTTSubscribeTopic subTopic = getTopic(topic);

  if (subTopic == MQTTSubscribeTopic::Heating) {
    callbackHeating(payload, length);
  } else if (subTopic == MQTTSubscribeTopic::Cooling) {
    callbackCooling(payload, length);
  } else if (subTopic == MQTTSubscribeTopic::Power) {
    callbackPower(payload, length);
  } else if (subTopic == MQTTSubscribeTopic::SmartGrid) {
    callbackSg(payload, length);
  } else if (subTopic == MQTTSubscribeTopic::CAN) {
    callbackCAN(String(topic).substring(mqttCANTopicLength()), payloadText, length);
  } else if (subTopic == MQTTSubscribeTopic::Safety) {
    callbackSafety(payload, length);
  } else {
    debugSerial.printf("Unknown topic: %s\n", topic);
  }
}

void mqttInitCallbacks()
{
  mqttSetCallback(callback);
}