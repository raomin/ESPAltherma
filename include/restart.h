#ifndef ESPALTHERMA_RESTART_H
#define ESPALTHERMA_RESTART_H

#include <Arduino.h>

#ifdef ARDUINO_ARCH_ESP32
#include <esp_attr.h>
#include <esp_system.h>
#include "eventlog.h"

// Why the firmware restarted itself, kept across the restart in RTC memory (lost on power loss).
#define RESTART_CAUSE_MAGIC 0x52535443UL
RTC_NOINIT_ATTR uint32_t restartCauseMagic;
RTC_NOINIT_ATTR char restartCauseSaved[128];
char restartCause[128] = ""; // cause of the restart that started this boot, "" when unknown

// Call once at boot.
void restartCauseLoad()
{
  esp_reset_reason_t r = esp_reset_reason();
  if (restartCauseMagic == RESTART_CAUSE_MAGIC && r != ESP_RST_POWERON && r != ESP_RST_BROWNOUT)
  {
    restartCauseSaved[sizeof(restartCauseSaved) - 1] = 0;
    strlcpy(restartCause, restartCauseSaved, sizeof(restartCause));
  }
  restartCauseMagic = 0;
}

void restartCauseSave(const char *cause)
{
  strlcpy(restartCauseSaved, cause, sizeof(restartCauseSaved));
  restartCauseMagic = RESTART_CAUSE_MAGIC;
}
#endif

void restart_board(const char *cause = nullptr)
{
  #if defined(ARDUINO_ARCH_ESP8266)
  system_restart();
  #else
  eventAddf("Restart: %s", cause != nullptr ? cause : "requested");
  eventSave(); // the history must survive if the power goes next
  if (cause != nullptr)
    restartCauseSave(cause);
  esp_restart();
  #endif
}

#endif
