# ESPAltherma card

The live drawing of your heat pump installation from the [ESPAltherma](https://github.com/raomin/ESPAltherma) web page, in Home Assistant, plus a ready-made dashboard.

- **The drawing:**
  - the outdoor unit, the backup heater, the hot water tank and the house;
  - water pipes coloured by temperature, from blue (15°) to red (55°);
  - the fan spinning while the compressor runs, the 3-way valve's position and the water flow.
- **Taps:** tap a reading to open its history.
- **Room temperature:** the living room shows your thermostat's temperature and target when you have one (a `climate` entity).
- **Languages:** English, French, German, Italian and Spanish, following your Home Assistant language. It uses your theme's colours, light or dark.
- **Setup:** none. It finds the entities ESPAltherma publishes by itself.

## Install

**With HACS:**
1. In HACS, open the menu (⋮) → *Custom repositories*.
2. Add this repository with the type *Dashboard*.
3. Find **ESPAltherma card** and download it. HACS adds it to your dashboard resources.

**By hand:**
1. Copy `dist/espaltherma-card.js` to `config/www/`.
2. Add `/local/espaltherma-card.js` as a *JavaScript module* in Settings → Dashboards → ⋮ → Resources.

Then reload Home Assistant in your browser, bypassing its cache (Ctrl+Shift+R, or clear the app's cache on a phone). Until then the dashboard says *Timeout waiting for strategy element*.

## A whole dashboard

Create a new dashboard (Settings → Dashboards → *Add dashboard* → *New dashboard from scratch*). Open it, then ⋮ → *Edit dashboard* → ⋮ → *Raw configuration editor*, and replace everything with:

```yaml
strategy:
  type: custom:espaltherma
```

The dashboard holds:
- the drawing;
- your thermostat;
- the outdoor, hot water, compressor, flow, pressure, mode and error values;
- 24-hour history graphs of the water temperatures and the compressor.

It follows the entities you have, so it adapts when you select other values in ESPAltherma. To pick the thermostat yourself, add `climate: climate.my_thermostat` under `type`.

To customise it, open the dashboard's ⋮ menu, choose *Take control*, and edit the cards it generated.

## Just the card

```yaml
type: custom:espaltherma-card
```

Options:

| Option | Default | |
| --- | --- | --- |
| `climate` | `climate.altherma`, or the first climate entity named after Altherma, Daikin or a heat pump | the thermostat shown in the house |
| `entities` | found by their ESPAltherma ids | override one value, e.g. `entities: {out: sensor.my_outdoor_temp}` |
| `title` | none | card title |

Keys of `entities`:

| Key | Value |
| --- | --- |
| `out` | outdoor temperature |
| `lw1` | leaving water before the backup heater |
| `lw2` | leaving water after the backup heater |
| `inlet` | return water |
| `tank` | hot water tank |
| `r3t` | refrigerant |
| `room` | room temperature |
| `rtSp` | room setpoint |
| `dhwSp` | hot water setpoint |
| `lwSp` | leaving water setpoint |
| `inv` | compressor frequency |
| `amp` | compressor current |
| `v3` | 3-way valve |
| `pump` | water pump |
| `flow` | flow |
| `bar` | water pressure |
| `mode` | operation mode |
| `err` | error type |

## Thermostat

A thermostat needs ESPAltherma's thermostat relay (`switch.altherma`, see the ESPAltherma README) and a room temperature sensor. Create it in Settings → Devices & services → Helpers → *Create helper* → *Generic thermostat*:
- **Heater:** `switch.altherma`;
- **Sensor:** your room temperature sensor.

## Development

`src/espaltherma-card.js` is the source. `python build.py` writes `dist/espaltherma-card.js`, with the texts translated from ESPAltherma's web page translations (`web/i18n` of the ESPAltherma repository).
