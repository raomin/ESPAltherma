#ifndef ESPALTHERMA_GPIO_CONTROL_H
#define ESPALTHERMA_GPIO_CONTROL_H

// Free GPIOs declared as inputs or outputs from the web page or the REST API: a spare output for a relay board or
// a LED, an input for a contact. A declared pin always reads its live level, input or output. Only a free GPIO
// can be declared. Not free:
//   - the pins of the X10A port and the relays, and those of other features built in (the DS18B20 buses),
//   - the locked pins: by default the ones the board needs for itself (flash, PSRAM, USB, serial console,
//     strapping pins read at boot), editable in the settings,
//   - numbers the chip does not have, and input-only pins for an output.
// The declarations are kept in the settings and applied at start. When a pin becomes used or locked (the settings
// changed), it is released first and the new owner takes it over; its declaration waits until it is free again.
//
// No Arduino dependency: the hardware is an interface, so a unit test drives it with a fake.

#include <stdint.h>
#include <string.h>
#include <strings.h>

#define GPIO_CONTROL_MAX_PINS 64
#define GPIO_CONTROL_MAX_LOCKED 32
#define GPIO_CONTROL_MAX_DECLARED 16

#define GPIO_USE_LOCKED "locked"
#define GPIO_USE_NO_PIN "no such GPIO"
#define GPIO_USE_INPUT_ONLY "input only"

enum GpioMode : uint8_t
{
  GPIO_UNUSED, // not declared: left as the chip resets it
  GPIO_INPUT,
  GPIO_INPUT_PULLUP,
  GPIO_INPUT_PULLDOWN,
  GPIO_OUTPUT_LOW,
  GPIO_OUTPUT_HIGH,
  GPIO_MODE_COUNT
};

static const char *const GPIO_MODE_NAMES[GPIO_MODE_COUNT] = {"unused", "input", "input_pullup", "input_pulldown", "low", "high"};

inline const char *gpioModeName(GpioMode mode) { return mode < GPIO_MODE_COUNT ? GPIO_MODE_NAMES[mode] : "unused"; }

// The mode from its name; false when there is none of that name
inline bool gpioModeFromName(const char *name, GpioMode &mode)
{
  for (uint8_t m = 0; m < GPIO_MODE_COUNT; m++)
    if (name != nullptr && strcasecmp(name, GPIO_MODE_NAMES[m]) == 0)
    {
      mode = (GpioMode)m;
      return true;
    }
  return false;
}

inline bool gpioModeIsOutput(GpioMode mode) { return mode == GPIO_OUTPUT_LOW || mode == GPIO_OUTPUT_HIGH; }

// A declaration kept in the settings
struct GpioDeclaration
{
  int8_t pin;
  GpioMode mode;
};

// What the chip does with a pin (gpio_control_esp.h drives it with the ESP-IDF driver)
struct GpioDriver
{
  virtual ~GpioDriver() {}
  virtual bool exists(int pin) const = 0;
  virtual bool canOutput(int pin) const = 0;
  virtual void apply(int pin, GpioMode mode) = 0; // input (with its pull) or output (with its level)
  virtual void release(int pin) = 0;              // back to the chip's reset state
  virtual int read(int pin) const = 0;
};

struct GpioControl
{
  GpioDriver *driver = nullptr;
  const char *usedBy[GPIO_CONTROL_MAX_PINS];
  GpioMode mode[GPIO_CONTROL_MAX_PINS];
  bool planning = false;

  GpioControl()
  {
    for (int i = 0; i < GPIO_CONTROL_MAX_PINS; i++)
    {
      usedBy[i] = nullptr;
      mode[i] = GPIO_UNUSED;
    }
  }

  static bool inRange(int pin) { return pin >= 0 && pin < GPIO_CONTROL_MAX_PINS; }

  // A new plan of who uses which pin: beginPlan(), reserve() and lock() for each use, endPlan()
  void beginPlan()
  {
    for (int i = 0; i < GPIO_CONTROL_MAX_PINS; i++)
      usedBy[i] = nullptr;
    planning = true;
  }

  void reserve(int pin, const char *by)
  {
    if (inRange(pin) && usedBy[pin] == nullptr)
      usedBy[pin] = by;
  }

  void lock(const int8_t *pins, int count)
  {
    for (int i = 0; i < count; i++)
      reserve(pins[i], GPIO_USE_LOCKED);
  }

  // A pin declared from here that is now used or locked is released, before its new owner takes it over
  void endPlan()
  {
    planning = false;
    for (int pin = 0; pin < GPIO_CONTROL_MAX_PINS; pin++)
      if (mode[pin] != GPIO_UNUSED && usedBy[pin] != nullptr)
      {
        driver->release(pin);
        mode[pin] = GPIO_UNUSED;
      }
  }

  // Why a pin cannot take a mode, nullptr when it can
  const char *whyNot(int pin, GpioMode wanted = GPIO_INPUT) const
  {
    if (!inRange(pin) || !driver->exists(pin))
      return GPIO_USE_NO_PIN;
    if (usedBy[pin] != nullptr)
      return usedBy[pin];
    if (gpioModeIsOutput(wanted) && !driver->canOutput(pin))
      return GPIO_USE_INPUT_ONLY;
    return nullptr;
  }

  bool isFree(int pin) const { return whyNot(pin) == nullptr; }

  // False when the pin cannot take the mode
  bool set(int pin, GpioMode wanted)
  {
    if (planning || wanted >= GPIO_MODE_COUNT || whyNot(pin, wanted) != nullptr)
      return false;
    bool declared = mode[pin] != GPIO_UNUSED;
    if (wanted == GPIO_UNUSED)
    {
      if (declared)
        driver->release(pin);
    }
    else
    {
      if (!declared)
        driver->release(pin); // a GPIO function first; between two modes the pin is not reset (no glitch)
      driver->apply(pin, wanted);
    }
    mode[pin] = wanted;
    return true;
  }

  // The live level of a declared pin, -1 for the others
  int level(int pin) const { return inRange(pin) && mode[pin] != GPIO_UNUSED ? driver->read(pin) : -1; }

  // The declarations kept in the settings, applied where the pin is free and not in that mode yet. Returns the
  // number of declarations that wait for their pin.
  int applyDeclarations(const GpioDeclaration *list, int count)
  {
    int waiting = 0;
    for (int i = 0; i < count; i++)
    {
      const GpioDeclaration &d = list[i];
      if (inRange(d.pin) && mode[d.pin] == d.mode)
        continue;
      if (!set(d.pin, d.mode))
        waiting++;
    }
    return waiting;
  }
};

// The declarations with one pin changed: added, replaced, or removed (GPIO_UNUSED). Returns the new count.
inline int gpioDeclare(GpioDeclaration *list, int count, int pin, GpioMode mode)
{
  int at = -1;
  for (int i = 0; i < count; i++)
    if (list[i].pin == pin)
      at = i;
  if (mode == GPIO_UNUSED)
  {
    if (at < 0)
      return count;
    memmove(&list[at], &list[at + 1], sizeof(GpioDeclaration) * (count - at - 1));
    return count - 1;
  }
  if (at >= 0)
  {
    list[at].mode = mode;
    return count;
  }
  if (count >= GPIO_CONTROL_MAX_DECLARED)
    return -1; // full
  list[count].pin = (int8_t)pin;
  list[count].mode = mode;
  return count + 1;
}

#endif
