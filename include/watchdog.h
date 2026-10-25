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

// A stall of the main loop shorter than the watchdog's limit is still worth a line in the event history: the MQTT
// keepalive is missed from a few seconds on (the broker then drops the connection). The timer notes the stage the
// loop is stuck in; the loop reports it once it runs again.
#define WATCHDOG_STALL_MS 5000UL
volatile unsigned long watchdogStallSince = 0; // the beat a stall was noticed after (0: none)
static char watchdogStallStage[32];

static void watchdogCheck(void *)
{
  if (watchdogPaused)
    return;
  // Read the beats before millis(): the loop task beats on the other CPU meanwhile, and a beat newer than now
  // underflows now - beat to ~49 days and restarts a healthy board
  unsigned long loopBeat = watchdogLoopBeat;
  unsigned long hpBeat = watchdogHpBeat;
  unsigned long now = millis();
  if (now - loopBeat > WATCHDOG_STALL_MS && watchdogStallSince == 0)
  {
    strlcpy(watchdogStallStage, (const char *)loopStage, sizeof(watchdogStallStage));
    watchdogStallSince = loopBeat;
  }
  static char cause[96];
  cause[0] = 0;
  if (now - loopBeat > WATCHDOG_LOOP_MS)
    snprintf(cause, sizeof(cause), "Watchdog: the main loop stopped (in %s)", (const char *)loopStage);
  else if (hpBeat != 0 && now - hpBeat > WATCHDOG_HP_MS)
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

inline void watchdogLoop()
{
  unsigned long now = millis();
  if (watchdogStallSince != 0)
  {
    eventAddf("Main loop stalled %lu s (in %s)", (now - watchdogStallSince) / 1000, watchdogStallStage);
    watchdogStallSince = 0;
  }
  watchdogLoopBeat = now;
}
inline void watchdogHp() { watchdogHpBeat = millis(); }

#endif
