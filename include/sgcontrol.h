#ifndef ESPALTHERMA_SGCONTROL_H
#define ESPALTHERMA_SGCONTROL_H
#include "sgtimeout.h"

// Main loop / MQTT callback only. GPIO and publication stay on their existing owner task.
static SgCommandTimeout sgCommandTimeout;

static bool applySgMode(uint8_t mode, uint64_t now)
{
  if (!smartGridConfigured())
    return false;
  digitalWrite(config.sg1Pin, (mode & 2) ? sgActiveState() : sgInactiveState());
  digitalWrite(config.sg2Pin, (mode & 1) ? sgActiveState() : sgInactiveState());
  sgMode = mode;
  sgCommandTimeout.accepted(mode, now);
  return true;
}

void callbackSg(byte *payload, unsigned int length)
{
  uint8_t mode;
  if (!sgCommandMode(payload, length, mode))
  {
    mqttSerial.printf("Invalid Smart Grid command (%u bytes)\n", length);
    return;
  }
  if (!applySgMode(mode, esp_timer_get_time() / 1000))
    return;
  static const char *const names[] = {"Normal operation", "Forced OFF", "Recommended ON", "Forced ON"};
  mqttSerial.printf("Set SG mode to %u - %s\n", (unsigned)mode, names[mode]);
  publishSgState();
}

// Runs even when MQTT is disconnected. Only a valid received SG command renews the interval.
void sgTimeoutLoop()
{
  if (!smartGridConfigured())
  {
    sgCommandTimeout.cancel();
    return;
  }
  if (sgMode == 0 || config.sgTimeoutSeconds == 0)
    return;
  sgCommandTimeout.configure(config.sgTimeoutSeconds);
  uint64_t now = esp_timer_get_time() / 1000;
  if (!sgCommandTimeout.takeExpired(now))
    return;
  auto age = (unsigned long long)sgCommandTimeout.ageSeconds(now);
  applySgMode(0, now);
  mqttSerial.printf("Smart Grid: normal operation after %llu s without a command\n", age);
  eventAddf("Smart Grid timeout: normal operation after %llu s without command", age);
  publishSgState();
}
#endif
