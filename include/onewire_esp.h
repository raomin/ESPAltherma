#ifndef ESPALTHERMA_ONEWIRE_ESP_H
#define ESPALTHERMA_ONEWIRE_ESP_H

// The 1-Wire time slots of onewire_temp.h on a GPIO, with the ESP-IDF GPIO driver: the same code on every ESP32
// (the OneWire library's direct register access does not build for the C6). Standard speed, the timings of
// Maxim application note 126. The pin is open drain: 0 pulls the line low, 1 lets the 4.7 kOhm pull-up raise it.
// The sensors' supply can come from a GPIO as well (EspOneWirePower).
// Interrupts are held off for one slot at most (about 70 us), never for a whole byte.

#include <Arduino.h>
#include "driver/gpio.h"
#include "esp_rom_sys.h"

#include "onewire_temp.h"

#define ONEWIRE_RESET_LOW_US 480
#define ONEWIRE_PRESENCE_SAMPLE_US 70
#define ONEWIRE_RESET_RECOVERY_US 410
#define ONEWIRE_WRITE_ONE_LOW_US 6
#define ONEWIRE_WRITE_ONE_RELEASE_US 64
#define ONEWIRE_WRITE_ZERO_LOW_US 60
#define ONEWIRE_WRITE_ZERO_RELEASE_US 10
#define ONEWIRE_READ_LOW_US 3
#define ONEWIRE_READ_SAMPLE_US 10
#define ONEWIRE_READ_RECOVERY_US 53

struct EspOneWireBits : OneWireBits
{
  gpio_num_t pin = GPIO_NUM_NC;
  portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

  void begin(uint8_t gpio)
  {
    pin = (gpio_num_t)gpio;
    gpio_reset_pin(pin);
    gpio_set_direction(pin, GPIO_MODE_INPUT_OUTPUT_OD);
    gpio_set_pull_mode(pin, GPIO_PULLUP_ONLY); // weak, the external 4.7 kOhm does the work
    gpio_set_level(pin, 1);
  }

  bool resetPulse() override
  {
    gpio_set_level(pin, 0);
    esp_rom_delay_us(ONEWIRE_RESET_LOW_US);
    portENTER_CRITICAL(&mux);
    gpio_set_level(pin, 1);
    esp_rom_delay_us(ONEWIRE_PRESENCE_SAMPLE_US);
    bool presence = gpio_get_level(pin) == 0;
    portEXIT_CRITICAL(&mux);
    esp_rom_delay_us(ONEWIRE_RESET_RECOVERY_US);
    return presence;
  }

  void writeBit(bool bit) override
  {
    portENTER_CRITICAL(&mux);
    gpio_set_level(pin, 0);
    esp_rom_delay_us(bit ? ONEWIRE_WRITE_ONE_LOW_US : ONEWIRE_WRITE_ZERO_LOW_US);
    gpio_set_level(pin, 1);
    portEXIT_CRITICAL(&mux);
    esp_rom_delay_us(bit ? ONEWIRE_WRITE_ONE_RELEASE_US : ONEWIRE_WRITE_ZERO_RELEASE_US);
  }

  bool readBit() override
  {
    portENTER_CRITICAL(&mux);
    gpio_set_level(pin, 0);
    esp_rom_delay_us(ONEWIRE_READ_LOW_US);
    gpio_set_level(pin, 1);
    esp_rom_delay_us(ONEWIRE_READ_SAMPLE_US);
    bool bit = gpio_get_level(pin) != 0;
    portEXIT_CRITICAL(&mux);
    esp_rom_delay_us(ONEWIRE_READ_RECOVERY_US);
    return bit;
  }
};

// One bus: the time slots on a GPIO and the protocol over them
struct EspOneWireBus
{
  EspOneWireBits bits;
  OneWireProtocol protocol;

  void begin(uint8_t gpio)
  {
    bits.begin(gpio);
    protocol.bits = &bits;
    protocol.resetSearch();
  }
};

// The sensors' supply from a GPIO driven high (a DS18B20 takes 1.5 mA at most while converting, a GPIO gives 20 mA
// at full drive). Off drives the data lines low as well: through the pull-up, they would keep the sensors running.
struct EspOneWirePower : OneWirePower
{
  gpio_num_t pin = GPIO_NUM_NC;
  gpio_num_t dataPins[ONEWIRE_MAX_BUSES];
  size_t dataPinCount = 0;

  void begin(uint8_t gpio)
  {
    pin = (gpio_num_t)gpio;
    gpio_reset_pin(pin);
    gpio_set_direction(pin, GPIO_MODE_OUTPUT);
    gpio_set_drive_capability(pin, GPIO_DRIVE_CAP_3);
    gpio_set_level(pin, 1);
  }

  // Takes over a supply GPIO that is already on, without switching it
  void adopt(uint8_t gpio) { pin = (gpio_num_t)gpio; }

  void addDataPin(uint8_t gpio)
  {
    if (dataPinCount < ONEWIRE_MAX_BUSES)
      dataPins[dataPinCount++] = (gpio_num_t)gpio;
  }

  void set(bool on) override
  {
    if (on)
      gpio_set_level(pin, 1);
    for (size_t i = 0; i < dataPinCount; i++)
      gpio_set_level(dataPins[i], on ? 1 : 0); // open drain: 0 pulls the line low, 1 releases it
    if (!on)
      gpio_set_level(pin, 0);
  }
};

#endif
