# ESPAltherma 2 – Review and Improvement Plan

**Status:** Branch `beta`, commit `505b7ab` (identical to `raomin/ESPAltherma:beta`), review dated 10 November 2026.

## 1. Scope and Approach

**Read in full:** `src/main.cpp`, `include/control.h`, `comm.h`, `config.h`, `converters.h`, `mqtt.h`, `mqttserial.h`, `topics.h`, `netmgr.h`, `netpolicy.h`, `webserver.h`, `stats.h`, `catalog.h`, `survey.h`, `restart.h`, `watchdog.h`, `stage.h`, `eventlog.h`, `eventring.h`, `logbuf.h`, `labeldef.h`, `board.h`, `fwupdate.h`, `haremoval.h`, `homeassistant.h`, `power.h`, `src/homeassistant.cpp`, `web/index.html`, `scripts/gen_catalog.py`, `scripts/build_web.py`, `telemetry/functions/api/*.js`, `telemetry/schema.sql`, `platformio.ini`, `.github/workflows/build.yml`.

**Only skimmed:** `detect.h`, `layoutfix.h`, `diii_comm.h`, `improv.h`, the tests, and the documentation.

**Not read:** `flasher/`, `contrib/`, `scripts/webui_mock.py`, `scripts/check_i18n.py`, `release.yml`.

**Not done:** I did not build the firmware or run the native tests. Findings marked as "inferred from code" have not been reproduced on a device.

**Additional sources:**

- Claude session `2e24093b` (commissioning on an EHBX08EF6V + ERGA06EVH, Hydro software 20002203, PRs #643–#656, incidents from 8 and 9 November).
- The running device (ESP32-S3, 192.168.2.234), read-only access: `/api/status`, `/api/events`, `/api/config`, `/api/detect`, `/api/values`.
- RAM dumps under `cl_ha/espaltherma2/` (`ram_dump_1.hex`, `ram_voll_2026-11-08.hex`, `ram_voll_2026-11-09.hex`). A new dump was not necessary: the three existing ones cover all addresses used by `control.h` for version 20002203.
- Three small verification scripts (catalog, topics/discovery buffers/profiles, RAM addresses). They are not in the repository; Section 8 proposes incorporating their checks as tests.

**Legend:** *verified* = demonstrated on the device, in a dump, or by script. *inferred* = follows from the code but has not been reproduced. Severity: high / medium / low.

## 2. Executive Summary

1. **"RAM only" protects the heat pump's memory, but writes to the ESP's flash on every target change** (the entire configuration plus the event history), while also pushing older events out of the history. Verified on the device (B1).
2. **The main loop stalls for 6–8 seconds in "publish values"**, because network transmission takes place while the values lock is held. The heat pump task also waits during this time. Verified on the device (B2).
3. **Long base topics break MQTT:** topics are truncated at 25 characters, Discovery JSON becomes invalid, and at 30 characters the power-limit command and state topics become identical. Verified by script (B3).
4. **Runtime state is stored in the configuration:** the held target, original curve, and limit are exported in backups and written back when restored. Concurrent changes made by the heat pump task can be lost (B4).
5. **"Restore curve" has no confirmation dialog** and is positioned directly next to "Apply" – consistent with the incident on 8 November at 15:43 (B6).
6. **Write endpoints have no protection against requests from external websites (CSRF)**, and a setup Wi-Fi network with a known password automatically starts after two minutes without a network connection (S1, S2).

What is good and should be preserved: the pure logic (control, detection, statistics, network policy) can be tested without Arduino and already is; web UI handlers do not directly modify state; writes to the heat pump go through a single task; there is a watchdog, event history, and firmware rollback. The profile table in `control.h` is internally consistent (all 15 software versions satisfy the same offsets, verified by script).

## 3. Bugs

### B1 – "RAM only": ESP flash wear and flooded event history (high, verified)

- **Where:** `src/main.cpp` `controlTick()`: `c.heldChanged` → `configSave()` (lines 855–862) and `eventAddf("%s target set to %d")` (lines 841–842); `include/eventlog.h` `eventLoop()` → `eventSave()`.
- **What happens:** Every new target in "RAM only" mode sets `held`, which is mirrored into `config.ctlHeld`, and the **entire** configuration is written to NVS as JSON (2338 bytes on the device, excluding passwords). In addition, every target change is added to the event history, which is saved as a blob of approximately 2.3 KB at least every two minutes. According to the Arduino core's `min_spiffs.csv` partition table, the NVS is 20 KB (based on memory; not locally verified).
- **Evidence:** In the device's event history, 9 of the 24 entries are "Leaving water target set to N" over just over four hours. Boot, network, and update events have already almost been pushed out.
- **Impact:** The option is explicitly promoted for frequently changing targets (automation based on electricity prices or PV surplus). This is precisely when ESP flash becomes a consumable component: with one change every five minutes, the nominal NVS page lifetime could be reached in a few years; with one change per minute, in significantly less than a year (estimate, not measured). The event history also loses its value for postmortem analysis.
- **Fix:**
  1. Separate the held target and limit state from the configuration (see B4) and store them under a dedicated, very small NVS key. Only write once the value has remained stable for, say, 10 minutes. Until then, keep it exclusively in RTC memory (which survives ESP restarts).
  2. Consolidate target changes in the event history ("Leaving water target 24 → 30, 9 changes") or give them the same token budget already used by `netHistory()`.

### B2 – Network writes while holding the values lock: main loop and heat pump task stall (high, verified)

- **Where:** `src/main.cpp` `publishValues()` (lines 788–802) → `updateValues()` → `mqttPublish()` for each value; `include/mqtt.h` `publishHomeAssistantDeviceDiscovery()` (entire Discovery stream while holding `valuesLock()`); `include/converters.h` `convert()` (serial output for each value, called while holding the lock).
- **Evidence:** Device event history: "Main loop stalled 8 s (in publish values)" (10 November, 02:42) and "... 6 s (in publish values)" (9 November, 20:25). The device uses "One topic per value", meaning 93 publishes per cycle while holding the lock.
- **Impact:** If a write operation hangs (Wi-Fi hiccup), `pollRegistries()` waits for `valuesLock()`. X10A queries, control ticks, and the time-critical DIII windows on the HT outdoor unit are delayed. MQTT keepalives and relay commands are also affected.
- **Fix:** Under the lock, only copy data (a snapshot of the `asString` values or label list). Then transmit without holding the lock. Remove the per-value serial trace `"Converting from: … -> …"` or put it behind a debug switch; at 115200 baud, this adds several hundred milliseconds per cycle while the lock is held.

### B3 – Base topic: truncated topics, invalid Discovery JSON, command echo (high, verified)

- **Where:** `include/topics.h` (topic buffer `char[48]`, base up to 32 characters); `include/mqtt.h` `publishControl()` (`json[640]`), `publishLimit()` (`json[640]`), `mqttConnectOnce()` (`json[1100]`).
- **Evidence (script):**
  - With a base topic of 25 characters or more: `…/power_limit/attributes` gets truncated; from 26 characters onward, the two target `…/attributes` topics are also truncated.
  - At approximately 25 characters: the target entity's Discovery JSON exceeds 640 bytes (515 at the default, 633 at 24, 662 at 28, 685 at 32 characters). `snprintf` truncates it, resulting in invalid JSON. The entity never appears, without any error message.
  - With a base topic of 30–32 characters: `power_limit/on/set` and `power_limit/on/state` become **identical** after truncation.
- **Impact of the last point (inferred):** The board subscribes to the topic on which it publishes its retained state. The retained "ON" message comes back as a command → `requestOn(true)` → `apply()` writes to the heat pump's RAM → the state is published again → an infinite loop of RAM writes.
- **Fix:** Derive buffer sizes from the maximum base-topic length (or limit the base to 24 characters and explain this in the UI). Stop formatting Discovery JSON into fixed-size buffers (see A8). Add a test in `test_topics`: "maximum base length: nothing truncated, all topics unique" (there are currently no length checks).

### B4 – Runtime state in the configuration: backup restore and lost updates (high, verified/inferred)

- **Where:** `include/config.h` (`ctlOrig`, `ctlHeld`, `ctlLimitOn/Value/Orig/Sw`, `telemetrySent`, `detectedKey`, `installId` in `configToJson`/`configFromJson`); `include/webserver.h` POST `/api/config` (lines 571–594) and `webLoop()` (`config = *next`, line 1189); `web/index.html` `cfgExport`/`cfgImport` (lines 1447–1455).
- **Evidence:** The device's `/api/config` contains `held_lwt: 30`, `orig_lwt_hi/lo: 25/38`, `orig_sw`, `limit_*`, `telemetry.sent`, and `install_id`. "Download a backup" saves exactly this response. "Restore a backup" sends the file back unchanged and without confirmation.
- **Impacts (inferred):**
  - An older backup restores an old held target or power limit; after the next reboot, the firmware writes it to the heat pump.
  - A stale "original curve" from a backup would be written to the heat pump's **memory** when "Restore curve" is used.
  - The same `install_id` ends up on two boards if a backup is restored to a second board.
  - Lost update: The handler copies `config` in the web task, and `webLoop()` applies the copy later. Any changes made and saved by the heat pump task in between (held target, original curve, detected model, layout corrections) are overwritten. The window is normally short, but grows with every main-loop stall (B2: 6–8 seconds).
  - `configSave()` is called from three tasks without a lock and serializes the structure while another task may be modifying it.
- **Fix:** Separate settings and runtime state (dedicated structures, separate NVS keys; runtime state must not be included in exports or accepted by the API). Apply changes field by field rather than replacing the entire structure, with a lock around reading, modifying, and saving. During restore: require confirmation, validate a version field, and display a notice saying "Passwords must be re-entered afterwards."

### B5 – Power limit: no early retry and no plausibility check before writing (medium, inferred)

- **Where:** `include/control.h` `LimitControl::tick()`/`identify()`/`readBack()`/`apply()`.
- **What is missing compared with `CurveControl`:**
  1. The staggered retry mechanism from #643 (30 seconds, then doubling). `LimitControl` only retries 10 minutes after a failure. After a shared power-up (ESP powered by X10A), a stored limit may therefore be reapplied up to 10 minutes too late.
  2. A plausibility check for the values read. `kind` is read once (`1` = kW, everything else = A). If the board responds before its RAM has loaded (according to comments in `control.h`, it then reads zeros), a kW limit is interpreted as amperes: `restore()` → `apply()` writes the ampere byte and sets the enable byte to 1, using a numerical value that was intended as kW. `kind` remains incorrect until the next identification (10 minutes); the "reapply" path calls `apply(value)` without `clamp()`.
  3. For profiles mapped through software analysis but "not yet confirmed on any device", there is no safety check at all before the first power-limit write.
- **Evidence for plausible ranges:** All three 20002203 dumps show Enable = 2, Type = 1, Limit 1 = 20.0 kW, 50 A, active limit 20.0 kW / 50.0 A.
- **Fix:** Only enter `READY` if Enable ∈ {0, 1, 2}, Type ∈ {0, 1}, and Limit 1 is within a physically plausible range (e.g. 1–25 kW or 4–75 A, not NaN). Otherwise report "not ready yet" and use the same backoff as the targets. Read Type on every `readBack()`. Add a test case for "response before RAM is loaded" in `test_control`.
- **Additional recommendation for targets:** The range records have the same structure in all dumps (`ff 00 <range> <minimum> 08`). Checking bytes 0, 1, and 4 as well would be a cheap second signature before the first write on an unconfirmed software version.

### B6 – "Restore curve": no confirmation, incorrect boolean check, no source attribution (medium, verified/inferred)

- **Where:** `web/index.html` lines 867–870 (button without `confirm`, directly next to "Apply"); `include/webserver.h` line 813 (`doc["lwt_restore"].is<bool>()`: even `false` triggers the action); `src/main.cpp` lines 836–843 (event does not record who initiated it).
- **Incident:** On 8 November at 15:43, the curve was restored, the held target was lost, and the heat pump oscillated until 16:30. Neither Home Assistant nor the session at the time triggered it; the event history only said "curve restored".
- **Fix:** Add a confirmation dialog explaining the consequences in plain language ("The 28 °C target will be abandoned, and the heat pump will follow its 25–38 °C curve again"). Visually separate the button. The API should only accept `true`. Record the source of every write command in the event history ("web 192.168.2.x" / "MQTT").
- Other actions also lack confirmation: "Restart" (deactivates the relays), "Undo the corrections", "Restore a backup", and saving modified pins (immediate restart).

### B7 – The homepage writes to the configuration just by being viewed (medium, inferred)

- **Where:** `web/index.html` `renderToday()` lines 713–715: `post('/api/config', {stats: {tz}})` whenever the browser's UTC offset differs from the stored timezone.
- **Impact:** Every daylight-saving time change, viewing from a phone abroad, or two visitors in different time zones causes a configuration write. If an admin password is configured (as it is on the device), this POST triggers the browser's password prompt on the homepage – precisely what the comment on the public `GET /api/config` endpoint was intended to avoid. Until someone opens the page, the statistics day rollover remains one hour off after a daylight-saving time change.
- **Fix:** Store a timezone rule once (using `Intl.DateTimeFormat().resolvedOptions().timeZone`, converted to POSIX-TZ) and use `configTzTime()`; do not write during rendering.

### B8 – Smart Grid with only one pin: entity appears, but nothing switches (medium, inferred)

- **Where:** `src/main.cpp` `setupPins()` configures outputs only when **both** pins are set; `include/mqtt.h` (Discovery, `callback()`, `callbackSg()`) and `statusJson()` only check `sg1Pin >= 0`.
- **Impact:** With only SG1 configured, the selection appears in Home Assistant and the mode is acknowledged, but no output is configured; `digitalWrite()` is called with pin −1.
- **Fix:** Use a `smartGridConfigured()` predicate (both pins required) everywhere. Reject the configuration when only one pin is set.

### B9 – Thermostat relay is not set to its safe idle level at startup (low to medium, inferred)

- **Where:** `src/main.cpp` `setupPins()`: `pinMode(config.thermPin, OUTPUT)` without first calling `digitalWrite` (Safety and Smart Grid handle this correctly); the stored state is only restored later in `readEEPROM()` after `setupScreen()` (a 2-second splash screen on M5 boards).
- **Impact:** An active-low thermostat relay is briefly energized at startup, for around 2 seconds on M5 boards. Combined with R4 (restart every 15 minutes when the broker is unavailable), this produces a recurring pulse.
- **Fix:** Write the level to be restored first, then call `pinMode`.

### B10 – Values JSON can silently disappear (low, verified by calculation)

- **Where:** `include/mqtt.h` `mqttPublish()` (silently returns `false` when the payload exceeds the buffer), `sendValues()`, `src/main.cpp` `updateValues()` (`snprintf` truncates, then the last character is blindly replaced with `}`; `strcat(jsonbuff, "]")` can overflow by one byte in table format).
- **Calculation:** 128 values with the longest names of an ECH2O model produce approximately 5.5 KB, plus statistics (approximately 0.8 KB) and unit counters (approximately 0.9 KB) – exceeding the usable 7.1 KB. Normal selections (93 values on the device) fit.
- **Fix:** Track the length instead of calling `strlen` for every value. On overflow, write a log entry and event, and display a warning on the Values page. Prefer sending multiple messages or streaming the data, as with Discovery.

### B11 – Duplicate names within a register (low, verified)

- "O/U MPU ID" appears in 17 definitions at 0xA0/10 **and** 0xA0/11; "Main pump" and "Add pump" appear in the two Multi-DHW tank definitions at 0x64/2 and 0x64/9. If both are selected, they share an MQTT key and Home Assistant ID; one hides the other.
- `scripts/gen_catalog.py` only checks whether the same name appears in multiple registers. Fix: extend the check to enforce "one name, one location" and make the three names unique.

### B12 – Minor bugs

| No. | Where | Bug | Fix |
|---|---|---|---|
| a | `converters.h` case 211 | `if (data == 0)` checks the pointer, not the byte: "OFF" is never returned | `data[0] == 0` |
| b | `converters.h` end of `convert()` | `dblData != NAN` is always true | `!isnan(dblData)` |
| c | `converters.h` case 100 | `strlcat(…, num)` uses the data length as the buffer size; unused in the catalog | Remove or correct the case |
| d | `converters.h` line 3 | Unused global `char buff[64]` | Remove |
| e | `webserver.h` `webLoop()` | `debug_serial` is missing from `mqttChanged`: subscription only takes effect after the next reconnect; a changed time server only takes effect after a restart, without notice | Include it in the change check or notify that a restart is required |
| f | `main.cpp` `statsSetup()` | The upper heat limit derived from device capacity is only set at startup and when statistics settings change, not after the first detection | Call again when the model changes |
| g | `stats.h` `resetDay()` | Clears the start-time ring at midnight: "Starts last hour" drops to 0 | Preserve the ring across day changes |
| h | `mqtt.h` SG Discovery | Device model hardcoded as "M5StickC PLUS ESP32-PICO"; inconsistent names ("EspAltherma Smart Grid", "Altherma", "AlthermaSensors") | Use `BOARD_NAME` and consistent names |
| i | `homeassistant.cpp` `discoveryStart()` | `"sw":"0.1"` hardcoded; no `sw_version` or `configuration_url` | Use `ESPALTHERMA_VERSION` and link to the web interface |
| j | `comm.h` `queryRegistryUnlocked()` | `millis() < start + SER_TIMEOUT` (not overflow-safe); a response completed exactly at the deadline is considered a timeout | Use the difference idiom and decide based on response length rather than time |
| k | `main.cpp` `hpTask()` | `HP_STAGE("hot water target")` covers both targets, limit, counters, memory reading, and DIII: watchdog message is misleading | Use a separate stage for each step |
| l | `main.cpp` `haExtraSetup()` | Overwrites the static list while the main loop may be reading it for Discovery; `haExtraCount = 0` is only set after overwriting | Populate a second list, then switch over under the values lock |
| m | `main.cpp` | `busy` is a plain `bool` but is used by two tasks | Make it `volatile`/atomic |
| n | `webserver.h` `collectBody()` | A body larger than 8 KB is reported as "invalid JSON" | Return a dedicated error (413) |
| o | `survey.h` / `main.cpp` | `surveyJson[3072]` is silently truncated when too long (invalid JSON for web UI, MQTT, and telemetry); 1093 bytes on the device, so currently well below the limit | Detect truncation and report it |

## 4. Robustness

### R1 – Pins are not validated; no safe startup mode (high, inferred)

`configFromJson()` and the handler do not check whether a pin exists on the board and is suitable as an output (flash pins, input-only pins, USB, strapping pins), or whether pins conflict (RX = TX, relay on an X10A pin). An invalid pin can crash the board during startup; the board then enters a reboot loop and can only be recovered via USB. Rollback only works after freshly flashed firmware. `frequency` is also unlimited server-side (the GUI enforces 5 seconds only in the browser; above 600 seconds, the statistics no longer integrate anything).

- Define an allowed GPIO table per board in `board.h`; validate server-side and reject invalid values with the relevant field name.
- Add a safe startup mode: maintain a counter in RTC memory; after three crashes shortly after startup, boot without X10A UART and relays, enable the setup Wi-Fi, and display a "Check pins" banner.

### R2 – Configuration: failed saves and corrupted data go unnoticed (medium, inferred)

`configSave()` does not check the result of `putString()` (NVS strings are limited to 4000 bytes; 128 values, four layout corrections, and passwords can approach this limit). `configLoad()` silently replaces an unreadable configuration with factory defaults and saves them.

- Check the result, report errors in the log, event history, and API response.
- Alternate between two keys (last known good state), add a version field, and log an event on read failure instead of silently overwriting the configuration.

### R3 – Unit counters: one outlier gets stuck; a restart makes them jump backwards (medium, verified/inferred)

- `MeterReader::readCounters()` retains the highest value ever read for each counter. A single excessively high reading (e.g. a read during an update) remains in place until the next ESP restart and is permanently added to Home Assistant's long-term statistics.
- **Evidence for the opposite case:** After the heat pump restarted, the heat-energy floats contained whole kWh values (9 November: 12,940.0 and 4,881.0; on 8 November they were still 12,940.573 and 4,881.828). The remembered maximum masks this only until the next ESP restart; afterwards Home Assistant sees a decrease.
- Proposal: accept an increase only after a second reading confirms it and it is consistent with elapsed time; keep maxima in RTC memory; reset them when the software ID changes.

### R4 – Restart policy when the broker is unavailable (medium, inferred)

"MQTT broker unreachable for 15 minutes" restarts the board – and then every 15 minutes again for as long as the broker remains unavailable (e.g. during maintenance on the Home Assistant host). The Wi-Fi policy has a rule for "never online since startup → 30 minutes"; the MQTT policy does not. Each restart resets Smart Grid to 0 and the Safety relay to inactive (neither is persisted; the thermostat relay is), triggers B9, and restarts all identification processes.

- Use staggered restart intervals (15 minutes, 1 hour, 6 hours), at most once as long as the web interface has been reachable.
- Explicitly decide and document which relays should retain their state across a restart; display this in the UI.
- Add exponential backoff to MQTT connection attempts (currently one blocking attempt every 5 seconds in the main loop).

### R5 – Home Assistant: stale values and disappearing entities (medium, inferred; one symptom verified)

- If the heat pump stops responding or a register becomes unavailable, its keys disappear from the JSON. Sensors retain their last values indefinitely, while Home Assistant logs template errors on every cycle. This is dangerous for Home Assistant automations that calculate values based on these sensors. → Use `expire_after` (e.g. three times the polling interval) and add a connection sensor called "Heat pump responding".
- If a target is not ready, its entity is **deleted** (empty Discovery), rather than marked "unavailable". Dashboards display "Entity not found", and the reason (`not_curve_mode`, `unsupported`, `no_answer`) is not visible in Home Assistant. During the session, the leaving-water target was frequently briefly "unavailable" at night. → Keep the entity, add a second availability topic per controller, and expose the status as an attribute or diagnostic sensor.
- A single failed exchange immediately sets the status to `no_answer`. → Require two or three consecutive failures first.

### R6 – Partially written curve, discarded command (low, inferred)

`CurveControl::apply()`: If point A succeeds and point B fails, half a curve remains in RAM. The command is discarded without retry or event logging (only the "RAM only" hold repairs this later). → Retry up to three times; otherwise log a "Write failed" event. Also, restoring in "RAM only" mode can call `persist()` if the stored state could not be read – contrary to the promise that "memory is never written".

### R7 – Missing sensors look like measurements (low, symptom verified)

`readRegistryValues()` does not check whether offset + size falls within the actual received response. Subsequent values are decoded as zeros and published as 0, −51.18 (pressure → temperature conversion of 0), or −40.6. On the device, 15 of 93 values are 0; it is impossible to distinguish "not present" from "actually 0". → Check response length (protocol I provides it) and publish "unknown"; treat known empty values in each conversion as "unknown".

### R8 – Additional points

- Blocking calls in the main loop: MQTT connection, MQTT test, broker discovery, telemetry, heartbeat, update check (TLS, 8-second timeout). Either move them into a dedicated network task or execute them strictly sequentially within a time budget.
- `heap_min` on the device is 58 KB; TLS calls only check current free heap (70 KB). Check the largest free block as well.
- Heat pump task stack (8 KB) is not measured; include `uxTaskGetStackHighWaterMark` in the status.
- An MQTT topic command beginning with "R" restarts the board; as a retained message, this causes a reboot loop on every connection. Only accept exact "RESET"/"REBOOT" commands and ignore them during the first few seconds after connecting.
- State stored in legacy `EEPROM` (thermostat) alongside NVS: consolidate.
- Web update does not set `busy`: polling and control continue while firmware is being flashed.

## 5. Security

### S1 – Write endpoints are not protected against requests from external websites (high, inferred)

The POST handlers (`/api/config`, `/api/control`, `/api/reboot`, `/api/factory-reset`, `/update`) do not check `Content-Type` or `Origin`/`Host`. Any website opened in the user's browser can issue requests via a simple form or `fetch` call to change targets, redirect the MQTT server (causing the board to connect to an external broker using its username and password), or upload firmware. This is unrestricted without an admin password; with a password, the browser sends its saved credentials. The same applies to DNS rebinding.

- For state-changing requests, validate `Origin` against the device's own host and require a custom header (sent by the page's `post()` function); enforce `Content-Type: application/json`.
- The telemetry backend already performs exactly this Content-Type check.

### S2 – Setup Wi-Fi with a known password starts automatically (medium, inferred)

`AP_PASSWORD "espaltherma"`; the setup network starts after two minutes without a network connection, even if Wi-Fi is configured (e.g. router restart). As long as no admin password is set, anyone within Wi-Fi range has access to the entire web interface, including firmware upload and write access to the heat pump. `ArduinoOTA` is also open without an admin password.

- Explicitly offer admin password setup in the configuration wizard.
- Add an option "Setup Wi-Fi only when no network is configured"; use a per-device password (derived from the MAC address and displayed via Serial/display) or make it configurable.

### S3 – Additional points (low)

- `POST /api/discover` (blocks the main loop for several seconds) and `GET /api/wifi/scan` (starts scans) do not require authentication.
- "MQTT ↔ serial gateway": any client on the broker can send arbitrary X10A frames, including memory write commands. The GUI does not explain this risk. Reception holds the serial lock as long as bytes continue to arrive (without an upper limit) and calls `client.loop()` recursively.
- TLS everywhere uses `setInsecure()` (MQTT, telemetry, update checks). Provide an optional CA certificate for MQTT.
- Firmware upload does not check whether the image was built for this board (same chip, different pins). Check the project/board identifier in the image; the GUI can compare the filename against `status.board`.
- Telemetry: the daily limit is global; someone can fill it with random IDs, preventing legitimate reports for the rest of the day.

## 6. GUI Usability

| No. | Observation | Suggestion |
|---|---|---|
| G1 | No confirmation dialogs for consequential actions (B6). | Consistent confirmation dialog explaining the consequences in plain language; visually separate dangerous buttons. |
| G2 | "Experimental": three dense paragraphs; prerequisites only become visible on the homepage after saving. During the session: "Do I need to change anything on the heat pump first?" | Add a live checklist for each feature next to its checkbox: supported software ID, target mode must be weather-compensated (include menu path on the controller), permitted range. Reduce the text to two sentences and put the rest under "More". |
| G3 | Field settings such as [9-00], [C-08], [4-08] are mentioned without explanation. During the session: "4-08 … how can I read that?" | Short glossary or tooltip with names and menu paths; display the value wherever the firmware already knows it. |
| G4 | Pins are plain numeric fields; "-1 = unused" is only documented for relays. During the session, board labels (16/17 versus GPIO17/18) caused wires to be swapped. | Per-board selection list with warnings (strapping, USB, input-only), collision checks, and "Swap RX/TX" and "Test connection" buttons directly on the card; only restart after confirmation. |
| G5 | Settings: one long form, one Save button at the bottom, no indication of unsaved changes; switching tabs or languages discards input. | Save per card or show an "unsaved" indicator with a warning when leaving. |
| G6 | Values: grouped by "Registry 0x61"; changing routes reloads the catalog and discards unchecked selections. | Group by meaning (temperatures, pressures, states, counters), with register view as a toggle; preserve selections when reloading. |
| G7 | Nothing indicates which values are required by the diagram, statistics, and Home Assistant cards. Unchecking "Flow sensor" results in "—" without explanation. | Mark values as "Required for homepage/statistics" and warn when deselecting them; the "Today" card should identify the missing input. |
| G8 | Explanations are only available as `title` tooltips, which cannot be accessed on mobile. | Use a tappable info icon or expandable text. |
| G9 | After a firmware update or restart, the old page remains displayed (old script running against new firmware). | Wait for the new build identifier and reload; show a banner if the identifiers differ. |
| G10 | Firmware file: any `.bin` is accepted. | Validate filename against board, reject "factory" images, display a "Do not close this page" warning. |
| G11 | Reading memory: maximum of 64 bytes per request. Contributors end up writing their own scripts (session: `speicher_lesen.py`; multiple dumps in #599). | Add "Download range as file" with progress; use the format described in the documentation. |
| G12 | With an admin password configured, an unauthenticated visitor can see all controls; errors only appear after clicking. | Clearly indicate read-only mode and provide a login button. |
| G13 | Values from absent sensors (0, −51.18) appear like real measurements (R7). | Display as "—" as soon as the firmware returns "unknown". |
| G14 | There is no place showing what Home Assistant receives (device name, base topic, number of entities, status of target entities). | Add a small "Home Assistant" card to the system page. |

## 7. Architecture

### A1 – One translation unit, global variables, include order

Almost everything is implemented in headers (`comm.h`, `mqtt.h`, `netmgr.h`, `webserver.h` define globals and non-inline functions) and only works because `main.cpp` includes them in the correct order. Dependencies are managed through `extern` and repeated forward declarations (`valuesLock()` is declared four times across three files). Proposed gradual approach: move Arduino-dependent parts into `.cpp` files with narrow headers (start with `comm`, `mqtt`, `webserver`); leave pure logic headers as they are.

### A2 – Split up `main.cpp`

1,200 lines covering the display, label construction, survey process, layout correction, statistics integration, Home Assistant additional values, control integration, memory diagnostics, task, and setup. Goal: `setup()` and `loop()` should call a few clearly named steps; one module per topic (`hp_task`, `survey_run`, `stats_glue`, `ha_extras`, `control_glue`, `memory_diag`, `screen`).

### A3 – Use semantic roles instead of name comparisons

The role of a value is guessed from its English name in five places: `statsValue()` prefixes (`main.cpp`), the homepage's `KEYS` regexes, units in `homeassistant.cpp` ("l/min", "(A)", "(rps)"), two-zone names in `catalog.h`, and size types in `detect.h`. They are already inconsistent: "Leaving water temp. (hydrobox)" is considered "before the heating element" in the firmware, but "after" on the homepage.

Proposal: Have `gen_catalog.py` assign a role and unit to each entry (using a table in the script, validated during generation). Statistics, homepage (`/api/values` includes the role), and Discovery should all use this metadata.

### A4 – One decoder

Raw bytes are decoded three times: `Converter::convert()`, `detectDecode()`, and `layoutDecode()`. Move the linear conversions (101–118, 151–165) into a table of `{signed, byte order, factor}` in an Arduino-free header used by all three. The 370-line `switch` then shrinks to special cases.

### A5 – Configuration

Each setting is defined in four places (structure, default value, `toJson`, `fromJson`), plus its GUI field, translation, and manually maintained "has changed" comparisons in `webLoop()` (one field is already missing there, B12 e).

Proposal: Use a field table (`name, type, default, limits, requires restart, affected group`) as the single source of truth. Generate loading, saving, validation, and change groups from it. Add a version field with migrations instead of heuristics on every load, separate runtime state (B4), and introduce a lock.

### A6 – Unit identity and profiles

Four components (hot water, leaving water, limit, counters) each read the software ID themselves and look up their own profile (the lookup loop is duplicated three times in the code).

Proposal: Introduce a single `UnitIdentity` (software ID, profile, model name, "RAM loaded") that identifies the unit once and is shared by all components. This also provides a consistent solution to B5. Define profiles with named fields instead of positional lists of 13 hex numbers. Test the fixed offsets (B − A = 0x3E, leaving water = hot water + 2, type = enable + 1, counters = total electricity + 4, etc.).

### A7 – Control feedback

`control.h` reports state changes through flags that `main.cpp` polls and resets (`changed`, `heldChanged`, `origChanged`, `applied`, `released`, `stateChanged`). Introduce a small event structure ("target set", "curve restored", "hold changed", "write failed") with source attribution. This also makes the source attribution required by B6 possible.

### A8 – Discovery and JSON

Discovery messages are generated partly using `snprintf` into fixed-size buffers (B3), and partly using `std::string` with text searches on their own output (`find("\"p\":\"binary_sensor\"")`).

Introduce a small writer that appends fields, tracks length, and reports overflow for all entities. Represent sensor types as structured data (platform, device class, unit, state class) instead of text fragments. `STATS_LABELS` and the values in `statsAppendJson()` currently run in parallel with magic indices (`i >= 11 && i <= 13`); replace both with a table of `{name, type, decimal places, value}`.

### A9 – Web interface

One HTML file with around 1,000 lines of dense JavaScript, individual lines exceeding 400 characters, and no tests. Version comparison is implemented twice (`newerBuild()` in the page, `updateNewer()` in the firmware).

Proposal: Create modules under `web/src/`, combined by `build_web.py` (the script already embeds translations); add Node tests for pure functions (`reading()`, `versionRank()`, `newerBuild()`).

### A10 – Document concurrency

Three tasks (main loop, heat pump, web) plus network events and watchdog timers share `config`, values, control objects, `unitMeter`, and `stats`. The values and serial line are protected; everything else relies on convention.

A short document describing "who owns what and who is allowed to call what", plus assertions at entry points (check task handles), would prevent B2, B4, and B12 l from recurring.

## 8. Tests, Build, CI

- **New native tests:** Topics with maximum base length (B3); valid Discovery JSON within buffer limits; configuration round-trip including boundary values and size (R2); profile invariants (A6); `LimitControl` with response before RAM is loaded and backoff (B5); counter plausibility (R3); `queryRegistry` framing with echo, partial response, and deadline (B12 j); `LogRing`.
- **Generator:** "one name, one location" (B11); later, roles and units (A3).
- **Web interface:** Node tests for pure functions; a smoke test against `scripts/webui_mock.py`.
- **Build:** Pin libraries without fixed versions (`PubSubClient`, `M5Unified`; `ArduinoJson@^7.2.0`) to exact versions; enable warnings and report image sizes per board in CI.
- **Static analysis:** Run `cppcheck` or `clang-tidy` against Arduino-free headers (would have caught B12 a and b).

## 9. Proposed Implementation Order

The maintainer deliberately keeps the core small; therefore, changes should be delivered as small, individually mergeable PRs, with bugs fixed first.

| Package | Contents | Effort |
|---|---|---|
| 1 | B6 (confirmation, accept only `true`, source attribution in events), B12 a–e, g–k, n | Small |
| 2 | B3 (topic and Discovery buffers, length test) | Small |
| 3 | B1 (dedicated key for held target and delayed persistence, consolidate events) | Medium |
| 4 | B2 (snapshot instead of sending under lock, remove serial trace) | Medium |
| 5 | B5 and R5 third point (plausibility, backoff, debouncing) with tests | Medium |
| 6 | S1 (Origin/header validation), B7 (timezone), B8, B9 | Small to medium |
| 7 | B4 (separate runtime state, restore with confirmation and version) – builds on package 3 | Medium |
| 8 | R1 (pin table, validation, safe startup) together with G4 | Large |
| 9 | R4, R5 (restart policy, `expire_after`, retain entities instead of deleting them), R3 | Medium |
| 10 | GUI: G2, G3, G5–G9, G11–G14 in separate PRs | Small each |
| 11 | Architecture A3 (roles), A5 (field table), A6 (identity), then A1/A2/A4/A8/A9 incrementally | Large |

**Open questions for the maintainer before packages 8–11:** Whether a per-board pin table and safe startup mode should be part of the core, and whether semantic roles in the generator are desired.
