#ifndef ESPALTHERMA_POWER_H
#define ESPALTHERMA_POWER_H

// Power diagnostics: why the board last restarted and, on M5 boards, the supply and battery.
// A board powered by the X10A 5V can brown out: the reset reason tells, and the supply voltage shows the drops.

#include <Arduino.h>
#include <esp_system.h>

const char *resetReasonName()
{
  switch (esp_reset_reason())
  {
  case ESP_RST_POWERON: return "power on";
  case ESP_RST_EXT: return "reset pin";
  case ESP_RST_SW: return "restart by the firmware";
  case ESP_RST_PANIC: return "crash";
  case ESP_RST_INT_WDT:
  case ESP_RST_TASK_WDT:
  case ESP_RST_WDT: return "watchdog";
  case ESP_RST_DEEPSLEEP: return "wake from deep sleep";
  case ESP_RST_BROWNOUT: return "brownout (supply voltage dropped)";
  default: return "unknown";
  }
}

#if defined(ARDUINO_M5Stick_C_Plus2) || defined(ARDUINO_M5Stick_C_Plus) || defined(ARDUINO_M5Stick_C) || defined(ARDUINO_M5Stack_Tough)
#define HAS_POWER_MONITOR
// The X10A 5V reaches the board through a long thin cable and sits anywhere between 4.3 and 4.9 V, moving with
// every WiFi burst: only a real sag is worth an event, and only once it lasts. One event when the supply has been
// under LOW_SUPPLY_MV for LOW_SUPPLY_SAMPLES samples, one more when it has been back above SUPPLY_BACK_MV for
// SUPPLY_BACK_SAMPLES samples, with the lowest value seen in between: a supply that dips every few seconds logs two
// lines, not two hundred.
#define LOW_SUPPLY_MV 4200
#define SUPPLY_BACK_MV 4500
#define LOW_SUPPLY_SAMPLES 3   // 15 s at one sample every 5 s
#define SUPPLY_BACK_SAMPLES 12 // 60 s

// Last readings, sampled by the main loop: the power chip shares the I2C bus with the screen
struct PowerReadings
{
  int vbus = -1;     // external supply voltage [mV], -1 when the board cannot measure it
  int vbusMin = -1;  // lowest external supply voltage since boot [mV]
  int supplyCurrent = -1; // [mA], AXP192 boards only
  const char *source = ""; // "5V pin" or "USB"
  int battery = -1;  // [mV]
  int batteryCurrent = 0; // [mA], + charging / - discharging
  int batteryLevel = -1;  // [%]
  bool charging = false;
} power;

void samplePower()
{
  static unsigned long last = 0;
  static bool lowReported = false;
  static int lowCount = 0, backCount = 0, lowestMv = 0, dips = 0;
  if (last != 0 && millis() - last < 5000)
    return;
  last = millis();
  if (M5.Power.getType() == m5::Power_Class::pmic_axp192)
  {
    // Two inputs: the 5V pin (ACIN, eg. from the X10A) and USB (VBUS). The board runs on the higher one.
    float acin = M5.Power.Axp192.getACINVoltage();
    float vbus = M5.Power.Axp192.getVBUSVoltage();
    bool pin = acin >= vbus;
    power.vbus = (int)((pin ? acin : vbus) * 1000);
    power.supplyCurrent = (int)(pin ? M5.Power.Axp192.getACINCurrent() : M5.Power.Axp192.getVBUSCurrent());
    power.source = pin ? "5V pin" : "USB";
  }
  else
  {
    power.vbus = M5.Power.getVBUSVoltage();
    power.source = "USB";
  }
  power.battery = M5.Power.getBatteryVoltage();
  power.batteryCurrent = M5.Power.getBatteryCurrent();
  power.batteryLevel = M5.Power.getBatteryLevel();
  power.charging = M5.Power.isCharging() == m5::Power_Class::is_charging;
  if (power.vbus > 0)
  {
    if (power.vbusMin < 0 || power.vbus < power.vbusMin)
      power.vbusMin = power.vbus;
    if (power.vbus < LOW_SUPPLY_MV)
    {
      backCount = 0;
      if (lowReported)
      {
        if (power.vbus < lowestMv)
          lowestMv = power.vbus;
        if (lowCount == 0)
          dips++; // under again after a recovery that did not last
        lowCount++;
      }
      else if (++lowCount >= LOW_SUPPLY_SAMPLES)
      {
        mqttSerial.printf("External supply low: %.2f V on the %s, running on the battery (%.2f V)\n", power.vbus / 1000.0, power.source, power.battery / 1000.0);
        eventAddf("Supply low: %.2f V on the %s, battery %.2f V", power.vbus / 1000.0, power.source, power.battery / 1000.0);
        lowReported = true;
        lowestMv = power.vbus;
        dips = 0;
      }
    }
    else if (power.vbus >= SUPPLY_BACK_MV)
    {
      lowCount = 0;
      if (lowReported && ++backCount >= SUPPLY_BACK_SAMPLES)
      {
        if (dips)
          eventAddf("Supply back: %.2f V on the %s, after %d more dips down to %.2f V", power.vbus / 1000.0, power.source, dips, lowestMv / 1000.0);
        else
          eventAddf("Supply back: %.2f V on the %s, lowest %.2f V", power.vbus / 1000.0, power.source, lowestMv / 1000.0);
        lowReported = false;
        backCount = 0;
      }
    }
    else
      lowCount = backCount = 0; // between the two thresholds: neither side counts
  }
}
#else
void samplePower() {}
#endif

#endif
