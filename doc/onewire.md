# Temperature sensors (DS18B20): the details

The [README](../README.md#temperature-sensors-ds18b20) has the short version. This page keeps the rest.

## Wiring

Each bus under **Settings → Temperature sensors (DS18B20)** (**+ Add a bus**, up to 4) has a data GPIO, and optionally a supply and a ground GPIO. On each bus:

| Sensor wire | Board |
| ----------- | ----- |
| VDD (often red) | 3.3V |
| GND (often black) | GND |
| DATA (often yellow or white) | the GPIO of the bus, **and a 4.7 kΩ resistor from it to 3.3V** |

- Power the sensors from 3.3 V, not in parasitic mode (VDD to GND): the firmware does not drive the strong pull-up a parasitic conversion needs.
- One pull-up per bus, at the board. Sensors sold "with adapter board" often carry their own 4.7 kΩ (marked `472`): one of them on a bus is enough; four in parallel (1.2 kΩ) is about the lowest that still works at 3.3 V.
- Up to 16 sensors on up to 4 buses. On a bus, a line with short branches works best; a star of long arms (several metres each) can ring. With 3 m cables, 2 to 4 sensors per GPIO is a safe start; spread more over more GPIOs.
- Any free GPIO without a boot function works. Pins already used for the X10A port or a relay are refused (the log says so). On an ESP32-S3 DevKit, GPIO 4 to 7 or 15 to 18 (when not used for X10A) are good choices; avoid 0, 3, 45 and 46 (strapping) and 19/20 (USB).

### Supply from a GPIO

To put the three wires of a bus on one connector where the board has no 3.3V pin nearby, a GPIO can power its sensors: set it as the bus's **Supply GPIO**. It is driven high at start; VDD goes to it, and so does the pull-up (DATA to that GPIO, not to 3.3V). Each bus has its own: two connectors of three pins, say, for two cables.

| Sensor wire | Board |
| ----------- | ----- |
| VDD | the supply GPIO |
| GND | GND |
| DATA | the GPIO of the bus, **and a 4.7 kΩ resistor from it to the supply GPIO** |

- A DS18B20 takes 1.5 mA at most while converting; the GPIO is set to full drive (about 20 mA), plenty for the 16 sensors the firmware reads.
- When no sensor of a bus with its own supply answers any more (three cycles in a row, or a search that finds nothing), that supply is switched off for a second, the data line held low, and the buses are searched again; the other buses go on. A sensor hung by a glitch on a long cable comes back. At most every 10 minutes; `GET /api/onewire` counts it (`power_cycles`) and the log says so.
- The supply GPIO must be free (not X10A, a relay or a bus). On an ESP32-S3, GPIO 47 and 48 belong to the flash supply domain: on modules with 1.8 V flash or PSRAM (names ending in `V`, like `N16R16V`) they give 1.8 V only. Check with a multimeter, or use another GPIO.
- The ground can come from a GPIO as well (the bus's **Ground GPIO**), driven low with full drive: then all three wires can sit on three GPIOs next to each other. It stays low while the supply is switched off and on. A GPIO's low level is a few tens of millivolts above GND at 20 mA; with a few sensors (1.5 mA each) that is negligible. The ground GPIO must be free as well. A GPIO is taken once: a data pin already in use skips its bus, a supply or ground pin already in use is left out (3.3V and GND then), and the log says so.

## What happens

- At start (and with **Search the buses again**), each bus is searched; only DS18B20 and DS1822 sensors are kept.
- Every reading cycle (the same interval as the heat pump queries, 30 s by default), a conversion is started on all sensors of a bus at once, and each scratchpad is read 800 ms later. The main loop is not held for the conversion.
- A reading is kept only when its CRC is right. 85.0 °C, what a sensor holds just after a power-on, is kept only when the previous reading was within 5 K (a tank can be at 85 °C). After three cycles in a row without a good reading the sensor reads `null` (unknown in Home Assistant) until it answers again.

## Names, MQTT and Home Assistant

- A sensor without a name goes by its ROM code (`DS18B20: 28FF641E8216034C`). The name given on the settings page replaces it (`DS18B20: Tank top`); names are kept by ROM code, so a sensor keeps its name on another GPIO, and a sensor not found right now keeps its name for when it comes back.
- The readings are keys of `espaltherma/ATTR` (`"DS18B20: Tank top": 54.31`), next to the statistics, and temperature sensors of the "Daikin Altherma via ESPAltherma" device in Home Assistant (MQTT discovery), entity IDs from the label (`sensor.espaltherma_ds18b20_tank_top`). Renaming a sensor makes a new entity.
- `GET /api/onewire` lists the buses and the sensors with their ROM code, GPIO, name and reading; `POST /api/onewire/search` searches the buses again.
