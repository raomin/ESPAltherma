# DIII reader: Altherma HT outdoor unit

On an Altherma HT (EKHBRD hydrobox + ERSQ/ERRQ outdoor unit), the ESP connected to the outdoor A1P X41A port also reads the DIII-NET frames that pass between the hydrobox and the outdoor unit. Those frames carry values the outdoor registries don't have. The code is [include/diii_comm.h](../include/diii_comm.h). Its header maps every byte of the frames, with the names from Daikin's Service Diagnosis Tool and sample values.

It is read-only. It sends only `R` (RAM read, 16 bytes) and never writes anything. It never reads 0x0000–0x03FF or 0x2C00–0x2FFF.

## When it runs

- The selected model must be `Altherma(HT ERRQ-ERSQ outdoor unit 11-16kW)`. There is nothing to switch on.
- It waits 90 s after boot, so that an OTA update can always get in first. Then it reads the controller's software id at RAM 0x0680.
- Known software ids are listed below. On any other id it stops and checks again every 10 min (2 reads). The log then says `DIII reader: unsupported, software …`.
- It learns the phase of each frame in 1–3 min, then reads only in short windows, about 25–45 reads a minute. While it learns, the registry poll waits 7 s longer than set.
- The sensors are announced to Home Assistant once the unit is recognised.

## Values

| Key | Source | Notes |
|---|---|---|
| `E0: Leaving water (C)`, `E0: Return water (C)` | hydro E0, every 60 s | whole degrees |
| `E0: Remote room temp (C)` | hydro E0 | |
| `E0: Pump` | hydro E0 | "pump revolution speed" in Daikin's tool; a true rpm is not confirmed, see [Pump and flow](#pump-and-flow) |
| `E0: EVL (pls)`, `E0: EVH (pls)` | hydro E0 | R410A / R134a expansion valves |
| `E0: INV current (A)`, `E0: INV speed (rps)`, `E0: INV fin temp (C)` | outdoor E0 page 2 | compressor (secondary) current, not the mains current |
| `E0: Sync` | | 1 = hydro phase locked, 2 = outdoor, 3 = both |
| `63: Leaving water / Return water / R410A liquid / BRC1 room temp (C)` | 63 frame, ~5 min | 1/256 °C resolution |
| `RAM: Outdoor input current (A)`, `RAM: Outdoor rated current (A)`, `RAM: Outdoor load (%)` | RAM 0x2B73 / 0x2B79 / 0x2B7D | total mains current of the outdoor unit; used only when the three values agree |

A value is `null` when its frame has not been caught recently.

## Status by unit

| Unit | Outdoor software | E0 | 63 | RAM current |
|---|---|---|---|---|
| EKHBRD016ACV1 + ERSQ016 (selman61) | `DP0009_003` | ✔ | ✔ | ✔ |
| EKHBRD + ERRQ (stav242) | `DP0009_003` | ✔ (`Sync` 3 after 48 min) | the fix in `1c080ce` is not confirmed yet | ✔ (2.93 A, rated 27) |
| ERSQ016AV1 (julianbl) | `YP0211_004` | DIII slots at the same addresses, from Memory reads; not run yet | not run yet | self-checked, not run yet |
| EKHBRD + ERRQ (another user) | `YP0211_004` | Memory reads caught the outdoor E0 page 2 and the hydro E0 page 0x81 in the slots; not run yet | a 63 frame caught in the slot passes the checks; not run yet | self-checked, not run yet |

On these units the DIII system id (`d[1..4]` of each frame) differs: it is `01 00 00 00` on selman61's unit, `6B 53 E0 B8` on julianbl's and `92 59 E0 B8` on the ERRQ above. The reader does not depend on it.

## Log lines

| Line | Meaning |
|---|---|
| `DIII reader: ready, software DP0009_003` | the unit is recognised |
| `DIII reader: unsupported, software …` | other software: nothing more is read. Please report the id |
| `DIII reader: no answer` | the RAM read got no valid reply |
| `E0 hydro locked, phase … ms` / `E0 outdoor locked, …` | phase learnt |
| `63 locked, period … s` | 63 period learnt, 5–12 min after boot |
| `… lost, relearning` | 3 windows in a row without the frame |
| `63 rejected (…)` + `63REJ …` | a 63 frame failed the checks; logged once, please report |
| `RAM current not recognised …` + `CUR …` | the values at 0x2B70 don't agree, so the current stays null; logged once |

## Pump and flow

These are estimates only:
- **Flow:** about 5.5 L/min per 1000 of `E0: Pump` (range 4.5–6.6), from the R134a energy balance on selman61's unit at pump values 1287–1762. stav242's flow sensor measured 0 L/min at pump 0 and 23.55 L/min at pump 4100: a line through zero, 5.74 per 1000, so **flow ≈ 5.74 × `E0: Pump` / 1000 L/min**. Points in between are still needed to tell a line from a curve.
- **Pump power:** about 5 + 96 × (n / 4200)³ W, max 101 W. The cubic law is assumed.

More pairs of `E0: Pump` and measured flow, taken at the same moment, are welcome.

## Adding another unit

The frame layouts are standard DIII-NET, so they are the same on every unit. What can differ is where the outdoor controller keeps them in RAM. With **Diagnostics → Memory read** (read-only), please post:

1. `0x0680`, 32 bytes: the software id
2. `0x0C20`, 64 bytes, and `0x0C80`, 64 bytes, 2–3 times about 20 s apart, with the compressor running: the DIII slots (tx 0x0C25, rx 0x0C85 on the known units)

Please read only these addresses: that page reads any address you give it.

No outdoor firmware of these units is available to us. The addresses come from live RAM reads, and the current formula from the sibling VRV IV / EWYQ outdoor software.
