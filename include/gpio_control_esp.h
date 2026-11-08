#ifndef ESPALTHERMA_GPIO_CONTROL_ESP_H
#define ESPALTHERMA_GPIO_CONTROL_ESP_H

// The pins of gpio_control.h on the chip, with the ESP-IDF GPIO driver, and the pins each chip locks by default:
// the ones the board needs for itself. An output is input and output at once, so that its level reads back.

#include "driver/gpio.h"

#include "gpio_control.h"

// Locked by default (the settings can change the list): what the chip, and the board around it, need for themselves
//   ESP32:    0 2 5 12 15 strapping, 1 3 serial console, 6-11 flash
//   ESP32-C3: 2 8 9 strapping, 12-17 flash, 18 19 USB, 20 21 serial console
//   ESP32-C6: 4 5 8 9 15 strapping, 12 13 USB, 16 17 serial console, 24-30 flash
//   ESP32-S3: 0 3 45 46 strapping, 19 20 USB, 26-37 flash and PSRAM (33-37: octal PSRAM), 43 44 serial console
//   M5StickC, M5StickC Plus and Plus2, M5Stack Tough: all but the Grove port and the hat header (25 26 32 33 36):
//             the rest is inside the case (screen, power chip or power latch, buttons, IMU, IR, buzzer, flash)
//   M5Stack Atom: as the ESP32, and 16 17 flash (ESP32-PICO), 12 IR LED, 27 RGB LED, 39 button
//   WT32-ETH01: as the ESP32, and the Ethernet PHY: 16 its power, 18 23 MDIO MDC, 19 21 22 25 26 27 RMII (its clock
//             comes in on 0)
//   Olimex ESP32-PoE and the other Ethernet builds: as the ESP32, and the PHY: 12 its power, 16 17 its clock (or
//             the PSRAM), 18 23 MDIO MDC, 19 21 22 25 26 27 RMII
#if CONFIG_IDF_TARGET_ESP32S3
static const int8_t GPIO_BOARD_LOCKED[] = {0, 3, 19, 20, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 43, 44, 45, 46};
#elif CONFIG_IDF_TARGET_ESP32C3
static const int8_t GPIO_BOARD_LOCKED[] = {2, 8, 9, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21};
#elif CONFIG_IDF_TARGET_ESP32C6
static const int8_t GPIO_BOARD_LOCKED[] = {4, 5, 8, 9, 12, 13, 15, 16, 17, 24, 25, 26, 27, 28, 29, 30};
#elif defined(ARDUINO_M5Stick_C) || defined(ARDUINO_M5Stack_Tough)
static const int8_t GPIO_BOARD_LOCKED[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 21, 22, 23, 27, 34, 35, 37, 38, 39};
#elif defined(ARDUINO_M5Stack_ATOM)
static const int8_t GPIO_BOARD_LOCKED[] = {0, 1, 2, 3, 5, 6, 7, 8, 9, 10, 11, 12, 15, 16, 17, 27, 39};
#elif defined(ARDUINO_WT32_ETH01)
static const int8_t GPIO_BOARD_LOCKED[] = {0, 1, 2, 3, 5, 6, 7, 8, 9, 10, 11, 12, 15, 16, 18, 19, 21, 22, 23, 25, 26, 27};
#elif defined(HAS_ETHERNET)
static const int8_t GPIO_BOARD_LOCKED[] = {0, 1, 2, 3, 5, 6, 7, 8, 9, 10, 11, 12, 15, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27};
#else
static const int8_t GPIO_BOARD_LOCKED[] = {0, 1, 2, 3, 5, 6, 7, 8, 9, 10, 11, 12, 15};
#endif
static_assert(sizeof(GPIO_BOARD_LOCKED) <= GPIO_CONTROL_MAX_LOCKED, "the default locked pins fit the settings");

struct EspGpioDriver : GpioDriver
{
  bool exists(int pin) const override { return GPIO_IS_VALID_GPIO(pin); }
  bool canOutput(int pin) const override { return GPIO_IS_VALID_OUTPUT_GPIO(pin); }

  void apply(int pin, GpioMode mode) override
  {
    gpio_num_t gpio = (gpio_num_t)pin;
    if (gpioModeIsOutput(mode))
    {
      int level = mode == GPIO_OUTPUT_HIGH ? 1 : 0;
      gpio_set_level(gpio, level); // the level first, so that the pin never shows the other one
      gpio_set_direction(gpio, GPIO_MODE_INPUT_OUTPUT);
      gpio_set_pull_mode(gpio, GPIO_FLOATING);
      gpio_set_level(gpio, level);
      return;
    }
    gpio_set_direction(gpio, GPIO_MODE_INPUT);
    gpio_set_pull_mode(gpio, mode == GPIO_INPUT_PULLUP ? GPIO_PULLUP_ONLY : mode == GPIO_INPUT_PULLDOWN ? GPIO_PULLDOWN_ONLY : GPIO_FLOATING);
  }

  void release(int pin) override { gpio_reset_pin((gpio_num_t)pin); }

  int read(int pin) const override { return gpio_get_level((gpio_num_t)pin); }
};

#endif
