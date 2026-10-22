# Coming from ESPAltherma 1.x

- Install ESPAltherma 2 with the [web installer](https://espaltherma.pages.dev/), over USB. Your `setup.h` is not read anymore: enter the Wi-Fi when the installer asks, then the MQTT broker, the pins (if you did not use the default ones) and the relays in the web interface. The pins of the Smart Grid relays replace `PIN_SG1`/`PIN_SG2`.
- ESPAltherma 2 selects the recommended values of your model. Add the other values you had in your definition file in the Values tab.
- The MQTT topics are the same (`espaltherma/ATTR`, `espaltherma/POWER`, `espaltherma/sg/set`...), and so is the one-topic-per-value prefix `espaltherma/OneATTR/`, now a setting of Settings → MQTT output.
- **If you used an English definition file**, Home Assistant keeps the same device and the same entity IDs (`sensor.espaltherma_...`), so your history, dashboards and automations keep working for the values you select again.
- **If you used a translated definition file** (`def/French/`, `def/German/`...), your entity IDs and the attribute names of `sensor.althermasensors` came from the translated labels. ESPAltherma 2 always builds them from the English labels: only the displayed names follow the language (Settings → This device). So:
    - Home Assistant creates new entities, e.g. `sensor.espaltherma_leaving_water_temp_before_buh_r1t` instead of `sensor.espaltherma_auslass_phe_r1t`. Update your dashboards and automations.
    - To get rid of the old entities, [clear the discovery](home-assistant.md#device-discovery) once, then restart ESPAltherma: it publishes only the new ones. To keep the history of a sensor, you can then give the new entity the old entity ID (entity settings in Home Assistant).
    - Templates reading `state_attr('sensor.althermasensors', ...)` need the English attribute names, which the Values tab shows when the language is English.
- The ESP8266 is not supported by ESPAltherma 2: keep 1.x on it.
- To go back, build and upload ESPAltherma 1.x from the [v1 branch](https://github.com/raomin/ESPAltherma/tree/v1) as before.
