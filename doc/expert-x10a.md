# X10A expert API

This REST-only diagnostic interface sends arbitrary bytes, including commands that write RAM or persistent
memory. It does not validate addresses, commands, checksums or the unit's operating limits. A response means
bytes were received, not that a command succeeded. Interpret the protocol response and read the result back.

An admin password must be configured. Every endpoint requires HTTP Basic authentication (`admin` and that
password). POSTs additionally require `Content-Type: application/json` and `X-ESPAltherma-Expert: 1`.

1. `POST /api/expert {"enabled":true}` enables access for 15 minutes. Repeat to renew.
2. `POST /api/expert/x10a {"frame":"024CB1","timeout_ms":500,"idle_ms":100}` queues a software-ID read.
   The response is HTTP 202 with a transaction `id`. There is no implicit framing or checksum generation.
3. `GET /api/expert/x10a` returns status and the latest transaction. Poll until `done`, checking `id`.
4. `POST /api/expert {"enabled":false}` closes the session. It cancels a queued request; an already running
   exchange finishes and its reply remains readable. Restarting the ESP always disables the expert mode.

`GET /api/expert` also returns the status. No expert setting is saved to flash or included in backups.
Enabling/disabling is recorded in the event log. The existing web UI has no expert controls.

The result contains `tx`, `rx` (hex), `sent`, `received`, `started_ms` (ESP uptime when sending), `elapsed_ms`,
`timeout`, `truncated`, `cancelled`, `pending`, `running`, `done` and `expires_in_ms`.
Receive data includes any local echo, NAK, bad checksum and partial frame. Old UART input is discarded before
transmission. There are no automatic retries. Save replies on the caller: only the latest transaction is kept.

Limits: 256 transmit bytes, 512 receive bytes; excess receive bytes are drained and `truncated` is set.
`timeout_ms` is a total exchange deadline (20–2000 ms), including continuous input; `idle_ms` (5–timeout_ms)
ends reception after that much silence following a received byte. A timeout may contain a partial response.
Only one request may be queued/running (HTTP 409 otherwise); disabled access returns 403. Invalid requests
return 400. The heat pump task executes requests under the serial mutex, excluding ordinary polling and the
MQTT serial gateway. Other polling and controls continue between transactions; expert mode does not suspend
automations. Requests wait during firmware uploads.

Use `scripts/x10a_expert.py --url http://DEVICE --password-file PATH enable`, then the same arguments with
`frame "02 4C B1"`, and finally `disable`. `status` reads state. The helper never retries a write or silently
enables the mode. Its `frame(command, payload)` function builds protocol-I frames for Python experiments.

For software 20002203, `frame(0x52, bytes.fromhex("E8 2E"))` reads 16 RAM bytes beginning at the effective
DHW target. The first two payload bytes are a signed little-endian temperature times 256. Writing this
working value is experimental: the unit or its controller may replace it immediately.
