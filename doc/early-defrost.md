# Startup-defrost diagnostics

On supported protocol-I definitions, ESPAltherma records a defrost whose rising edge is observed within 15 minutes of a **known compressor start**. The preceding stopped reading must also place the start within that window. Booting while the compressor already runs does not establish a start. Starts during hot-water preparation count; a mode change with a continuously running compressor does not restart the window. The interruption belonging to defrost is suppressed as a new start. Cooling is excluded.

This detects timing, not ice. A low refrigerant/coil temperature during startup can accompany an ice-free heat exchanger. The notice calls an unnecessary defrost a possibility and asks the owner to check the outdoor heat exchanger **before** the next defrost. The owner can record `not_checked`, `ice_free` or `iced`; the firmware never infers ice-free status.

The Home notice and one saved case remain available for 24 hours after detection. A newer case replaces the older one. Storage is RAM only: restart, changing the model, or selecting an unsupported layout clears it. Two fixed traces preserve the saved case while another compressor start is monitored. The recorder occupies 1,888 bytes, has a compile-time ceiling of 2 KiB and is allocated only for a supported layout. Unsupported units still work normally, without this diagnostic.

## Read traffic and trace format

The layout must match the definition's public registry fields: operation/startup/defrost at `10`, outdoor/coil temperatures at `20`, and inverter frequency at `30`. Layout corrections must still supply these fields. No RAM writes or raw-command API are involved. Normal validated replies are reused; during an observed startup, these three pages are queried approximately every five seconds without extra retries. Start detection outside that window uses the normal polling interval.

The trace covers up to 15 minutes from the first observed running sample, including samples after detection. Downloads taken during recording are explicitly marked incomplete. Each of 181 entries occupies five bytes: delta seconds, outdoor and coil temperatures in 0.5 °C units, inverter rps, flags. Deltas use actual read times, not a synthetic five-second timeline. `-128` temperatures and `255` rps mean unknown. Missing operation feedback breaks start tracking after 60 seconds. Long gaps or capacity exhaustion mark the trace truncated. Reading the hydro board does not establish when the outdoor unit last refreshed those values.

`GET /api/defrost/download` streams an immutable JSON snapshot; `?format=csv` streams expanded rows. Only one download runs at once. JSON contains column names, units, flag meanings, uncertainty, reported device identifiers and the observation. CSV leaves unknown values empty. Neither download contains credentials or the installation identifier. A transient event copy and one formatting fragment are needed during a download; the entire trace is never expanded into an in-memory JSON array.

## Explicit case reports

`POST /api/defrost/observation` accepts the current `id` and an observation. `POST /api/defrost/report` additionally requires JSON boolean `user_consent: true`. Both use the configured administration authentication. Expired or replaced case IDs are rejected. The UI's send button makes this request; automatic telemetry/heartbeats never send cases. A failed attempt is shown and is not retried automatically.

Reports include the trace, observation, selected definition, firmware/board and reported device identifiers. A random installation identifier plus a boot identifier and event ID deduplicate retries. The receiver stores no IP or location. Maximum body size is 8 KiB, 181 samples, 20 cases per installation/day and 500 cases/day globally (`MAX_DEFROSTS_PER_DAY` can override the global ceiling). Data are owner submissions, not verified evidence of unnecessary defrost.

Sending temporarily requires an 8 KiB payload buffer, a snapshot, and HTTPS memory, in addition to the resident recorder. The existing TLS headroom check runs **after** allocating the payload; low memory fails the report without discarding the local case. Reporting does not promise a 2 KiB peak-memory budget.

The backend change requires a maintainer deployment: apply `telemetry/schema.sql` to the telemetry D1 database (the `CREATE TABLE/INDEX IF NOT EXISTS` statements are additive), then publish the Pages function `functions/api/defrost.js`. Firmware alone cannot enable a server endpoint. No migration or server deployment is performed by the firmware.

## Verification

Native tests cover start/defrost transitions, hot-water starts, continuous mode switches, boot mid-run, timing uncertainty, wraparound, gaps, two simultaneous traces, retention, packing and export rows. A public X10A trace from an ERGA startup reproduces a roughly seven-minute defrost at 2 °C outdoor air and a minimum coil reading of −10.5 °C without asserting an ice observation. The telemetry endpoint has Node tests for consent, boundaries, metadata filtering and limits. Run `python scripts/webui_mock.py --early-defrost` to inspect the UI with a synthetic case; its send button changes only the local mock.
