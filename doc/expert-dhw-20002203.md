# Fixed DHW target experiment on software 20002203

12 November 2026; EHBX08EF6V, ESP32-S3, firmware `2.0.0-expert.1`.
The test firmware is based on beta `911c0bf`, with the read-only installer-setting sensors from PR #655 retained.

The user selected fixed DHW mode and enabled DHW. Before writing, memory command I read index 278 as
`0x1A` (DHW weather-dependent bit clear) and index 77 as `0x1C` (space heating and DHW on).
RAM address `0x2EE8` contained `00 2F` = 47 C (signed little-endian value / 256).

Three single F commands wrote `00 14` = 20 C to `0x2EE8`. Each returned the valid acknowledgement `46 B9`.
Reads used command R, which returns sixteen bytes; each reply checksum was verified. No persistent-memory
write was issued. The neighboring words, including the 47 C at `0x2EEA`, remained unchanged in these reads.

| Trial | First read after write started | Next read | Result |
|---|---|---|---|
| 1 | +221 ms: 20 C | +560 ms: 47 C | Write applied, then overwritten |
| 2 | +119 ms: 20 C | +358 ms: 47 C | Write applied, then overwritten |
| 3 | +122 ms: 47 C | +360 ms: 47 C | Already restored by first read |

Times are ESP uptime timestamps at transmission of the respective frames, not precise times of an internal
RAM update. Later reads, including the final one about 25 s after the first write, all returned 47 C.
The original target was already restored by the unit, so the cleanup needed no write. Expert mode was disabled.
Normal polling still reported 93 selected values; DHW control correctly reported `not_curve_mode`.

Conclusion: direct writes to the effective fixed target are accepted and observable, but do not hold. This is
consistent with the controller-owned fixed setpoint. These traces alone cannot distinguish an incoming
controller message from periodic recomputation inside the hydro board. Other RAM locations were not tested.

[Full raw transactions and timestamps](experiments/2026-11-12-dhw-fixed.json).
The generic expert API used to collect these traces is proposed separately.

Validation: 151 native tests passed, including five expert-state/parser tests. ESP32-S3 build passed
(about 1.26 MB image, 32.9% static RAM). On-device API checks verified Basic authentication, disabled-mode
rejection, explicit header, strict enable boolean, malformed/oversize frames, timing limits and single-job
exclusion. Raw software identification returned `4C 03 22 00 20 6E` (20002203).
