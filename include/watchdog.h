#ifndef ESPALTHERMA_WATCHDOG_H
#define ESPALTHERMA_WATCHDOG_H

// Software watchdog: restarts the board when the main loop or the heat pump task stops making progress
// (a deadlock, a network call that never returns). The core's task watchdog only watches the idle task of
// CPU 0, so without this a stuck task leaves the device dead until it is power cycled.

#include <Arduino.h>
#include <esp_timer.h>
#include "restart.h"
#include "stage.h"

#define WATCHDOG_LOOP_MS 90000UL          // longest legitimate stall: an MQTT or TLS connection attempt, a few seconds
#define WATCHDOG_HP_MS (5UL * 60 * 1000) // the heat pump task beats between registries and while it waits

volatile unsigned long watchdogLoopBeat = 0;
volatile unsigned long watchdogHpBeat = 0;
volatile bool watchdogPaused = false; // ArduinoOTA upload: the loop is inside ArduinoOTA.handle()

static void watchdogCheck(void *)
{
  if (watchdogPaused)
    return;
  unsigned long now = millis();
  static char cause[96];
  cause[0] = 0;
  if (now - watchdogLoopBeat > WATCHDOG_LOOP_MS)
    snprintf(cause, sizeof(cause), "Watchdog: the main loop stopped (in %s)", (const char *)loopStage);
  else if (watchdogHpBeat != 0 && now - watchdogHpBeat > WATCHDOG_HP_MS)
    snprintf(cause, sizeof(cause), "Watchdog: the heat pump task stopped (in %s)", (const char *)hpStage);
  if (cause[0])
  { // not restart_board(): no NVS here, the stuck task may hold it. The RTC memory survives the restart.
    Serial.println(cause);
    eventAddf("Restart: %s", cause);
    restartCauseSave(cause);
    esp_restart();
  }
}

void watchdogBegin()
{
  watchdogLoopBeat = millis();
  esp_timer_create_args_t args = {};
  args.callback = watchdogCheck;
  args.name = "watchdog";
  esp_timer_handle_t timer;
  if (esp_timer_create(&args, &timer) == ESP_OK)
    esp_timer_start_periodic(timer, 5 * 1000 * 1000);
}

inline void watchdogLoop() { watchdogLoopBeat = millis(); }
inline void watchdogHp() { watchdogHpBeat = millis(); }

#endif
