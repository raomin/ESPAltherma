# Controlling the heat pump: the details

The [README](../README.md#controlling-the-heat-pump) has the short version of each control. This page keeps the rest.

## The thermostat relay

Adding the relay takes priority on your thermostat: ESPAltherma turns the heating on and off, the thermostat is in standby.

Note: I resoldered the J1 jumper that was cut when installing my digital thermostat (not sure if it is needed) and configured my *type of thermostat* as *External thermostat*.

On a ROTEX this connects to J16 pins 1 and 2. RT needs to be switched ON in the heat pump's Connection menu. Heating is ON if the pins are connected, else no heating, so connect to the NC (normally closed) contact of the relay.

Other users' installations are described [in this issue](https://github.com/raomin/ESPAltherma/issues/17).

## Smart Grid

The two relays open and close the SG1 and SG2 contacts of the heat pump; the mode is the combination of the two:

| sg/set value | SG1 | SG2 | SG mode | Working mode | Typical result |
| ------------ | --- | --- | ------- | ------------ | -------------- |
| 0 | open | open | 0 - normal operation | normal working mode | HP works like if SG features are disabled/not used |
| 1 | open | close | 1 - Forced OFF | HP is forced OFF | Heating and DHW will be turned OFF - *Beware that your comfort may be negatively affected by this working mode* |
| 2 | close | open | 2 - Recommended ON | HP is recommended to be ON | HP will increase DHW setpoint as well as LW setpoint (documentation says +5 °C, but my tests actually show +6 °C) |
| 3 | close | close | 3 - Force ON | HP is forced ON | HP will increase DHW setpoint and will use its full power to heat DHW (to 70 °C) |

*Note that in SG3 mode your HP will really be power hungry so make sure to enable it only when electricity cost is low (ideally free) or be prepared to get a high bill!*

Depending on your HP model, SG3 might be configurable in "ECO mode", "Normal mode" or "Comfort mode". The mode can be set using the specialist code: Main Menu > Settings > Input/Output.

| SG mode | Description |
| ------- | ----------- |
| Comfort mode | Increase of the hot water set temperature by 5 K. |
| Normal mode | Increase of flow set temperature by 2 K and hot water set temperature by 5 K. |
| ECO mode | Increase the flow set temperature by 5 K and hot water set temperature by 7 K. |

Smart Grid needs to be switched ON in the heat pump configuration menu, otherwise the SG1 and SG2 contacts are not evaluated.

Coming from v1: the pins set in Settings → Relays replace `PIN_SG1`/`PIN_SG2` of `setup.h`. Until they are set (-1), the Smart Grid select is not in Home Assistant. On a ROTEX, the SG1 and SG2 contacts are located in the J8 connector, pins 5-6 (Smart Grid) and 11-12 (EVU).

## Hot water and leaving water targets

The user interface of the heat pump owns the setpoints it shows and re-sends them to the hydro board every second, so they cannot be changed through the X10A port. But when a setpoint mode is *weather dependent*, the hydro board computes that target itself from its own curve. ESPAltherma can then write the wanted target into both points of that curve: the target holds whatever the weather, it applies within a second, and it is saved in the unit so it survives a restart. This works for the hot water tank and for the leaving water of the main zone (the water sent to the floor or the radiators).

To try it:

1. On the user interface of the heat pump, set the tank setpoint mode, or the leaving water setpoint mode of the main zone, to *weather dependent* ("loi d'eau" on a French interface).
2. In ESPAltherma, tick the corresponding box under **Settings → Experimental**.
3. The home page shows a *Hot water target* or *Leaving water target* card with the current target and the allowed range (the unit's own limits). Home Assistant gets a `Hot water target` and a `Leaving water target` number entity; the MQTT topics are `espaltherma/dhw_target/set` and `espaltherma/lwt_target/set` (a whole number of degrees), with the state on `.../state`.

Next to the target, the card shows what the thermostat really aims for, with an explanation on hover. The hot water target is capped by the maximum the heat pump reaches on its own at the current outdoor temperature (about 50 °C on a mild day; above that the booster heater would have to finish), so asking for 55 heats the tank to that maximum. The leaving water target carries the shift and the room modulation set on the unit. Home Assistant gets these as the `effective` and `ceiling` attributes of the entities (on MQTT, `.../attributes`).

The card tells you when it cannot work: the unit did not answer, the setpoint mode is not weather dependent, or the hydro board software is not supported. The addresses written are specific to the software of the hydro board, read from the unit at start; only the versions whose addresses are known are accepted, nothing is written on any other. Known so far: 18002904 (the 16 kW EAV/EAB DA, DF and DJ series, found on an EAVH16S18DA6V and tested there), and from the boards' software, each awaiting a first report: 17014006 (4-8 kW EHV/EHB DA and DJ), 17019A04 (4-8 kW ECH2O D), 20002203 (4-8 kW EHV/EHB E, 9-16 kW EBLA/EDLA monoblocs), 20007904 (12 kW ETV/ETB and ECH2O E), 20017704 (4-8 kW ECH2O E, 4-8 kW EBLA/EDLA E monoblocs), 20017705 (16 kW ETV/ETB and ECH2O E) and 22009C02 (12 kW ELV/ELB and ECH2O, 2026). If your unit shows "not available", please open an issue with the software version shown.

**Getting the unit's curve back.** ESPAltherma remembers the two points of the curve it found before its first write (kept across restarts, as long as the hydro board software is the same). The card then shows "Before ESPAltherma, the curve of the unit was ... " with a **Restore the unit's curve** button; Home Assistant gets a `Restore hot water curve` / `Restore leaving water curve` button entity, and `restore` sent to `espaltherma/dhw_target/set` or `espaltherma/lwt_target/set` does the same. The restore writes both points at once in the RAM and in the memory (the memory keeps its 5 minute interval), then ESPAltherma forgets them: the next target you apply captures the curve anew. Re-entering the curve on the user interface, or switching the setpoint mode to absolute, works too.

Things to know:

- The target is written in the unit's RAM at once, and in its memory 30 seconds later (at most every 5 minutes) so that a bouncing automation does not wear the memory.
- The hot water thermostat starts heating when the tank falls a few degrees under the target it aims for (the hysteresis set on the unit, 5 °C by default) and stops at that target. Raising the target by one degree moves both thresholds by one degree, nothing more.
- The user interface keeps showing its old curve until it restarts; saving that screen again on the interface writes the old curve back.
- Switching a setpoint mode back to *absolute* on the interface stops ESPAltherma writing that target; the interface's own setpoint applies again.
