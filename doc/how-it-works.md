# How ESPAltherma recognises your heat pump

All the definition files of [definitions](../definitions) (36 models) are merged at build time into one catalog of 656 distinct values (`scripts/gen_catalog.py`). One firmware therefore knows every model: it only has to find out which one it is connected to, and check that the definition really matches the unit.

## Detecting the heat pump

1. **Protocol.** ESPAltherma asks registry `0x60` in protocol I. If nothing answers, it tries registry `0x53` in protocol S (older units).
2. **Survey.** It reads once every registry the definition files use (on protocol I, `0x61`, `0x62`... are walked until the first missing one, like the service tools do). The result is the detection report, published on `espaltherma/detect` and shown in Diagnostics → Heat pump survey.
3. **Identification.** Registry `0x63` holds the AS number of the indoor board, as printed on its label: `AS1708171-30 F` reads `01 70 81 71 03 06`. ESPAltherma looks it up in a table of 731 known AS numbers and the models they are fitted in ([data/as_numbers.json](../data/as_numbers.json)), which gives the candidate definitions. The AS number, the software ID (`0x60`), the outdoor capacity (`0x00`, offset 12, in kW x10) and the outdoor board part number (`0x11`) together form an identification key.
4. **Known units.** A key listed in [data/fingerprints.json](../data/fingerprints.json) names the model directly. The table grows from the reports users share (the box ticked when confirming the model, see [Step 3 of the README](../README.md#step-3-finishing-in-the-web-interface)).
5. **Scoring.** The candidates (every model when the AS number is unknown) are scored against the survey:
    - the registries the model reads must answer, and the family-specific ones it does not know (`0xA0`, `0xA1`, `0x65`) should not;
    - replies must be at least as long as what the model reads, ideally exactly as long;
    - the values at the model's temperature, voltage, current and flow slots must be physically plausible;
    - the outdoor capacity must be in the range of the definition.
6. **Confidence.** Models that read the recommended values the same way count as one. On protocol I, the confidence is **high** only with a known key or a single AS number candidate: the model is then used right away. (Protocol S has two definitions, told apart by the registries that answer.) Otherwise (the DA and DJ series, for instance, share their AS numbers) you confirm the model in Setup → Heat pump. Until then, ESPAltherma only publishes the *safe values*: the recommended values that every close candidate reads identically (same registry, offset, conversion and size).

The survey runs at every start. If another heat pump answers (the key changed), the model is detected again. While no model is known, the survey is retried every minute, in case the cable was plugged in after the board.

## Correcting wrong definitions

Some definitions do not match every unit of their series. In several registries (`0x00`, `0x21`, `0x30`, `0x61`...) the definitions place 2-byte values at different offsets. With the wrong layout, each value is read across two neighbours: absurd numbers that jump by 25.6 each time the neighbouring byte changes by one. On a real EPGA16, registry `0x21` matches the Gen-1 layout, not its own definition.

For those registries, ESPAltherma scores every candidate layout, taken from the other definitions, on each poll:

- **plausible:** the values are in a physical range;
- **steady:** they change little between two polls;
- **mirrored:** they equal a sensor of another registry (the heat pump often serves the same sensor twice).

A registry switches to another layout only on clear evidence:

- after at least 4 polls, the definition's own layout reads fewer than 90% plausible values;
- the other layout is always plausible, never jumps and reads real (non-zero) values;
- this is backed by jumps of the current layout or by mirrored values.

Registries without evidence, for instance all zero while the unit is idle, are never touched.

The corrected registry is then read with the layout (and the labels) of that other definition. Corrections are logged and listed in Diagnostics → Definition corrections, where you can undo them or turn off *Correct automatically*. They are cleared when the model changes.

Sharing your report when you confirm the model (or posting your detection report and your exact model in an issue) helps: it adds your unit to the known keys, and a correction found on your unit shows which definition needs fixing.

## The protocols

The serial protocols themselves are documented in [Daikin I protocol](Daikin%20I%20protocol.md) and [Daikin S protocol](Daikin%20S%20protocol.md), and the AS number bytes in [AS_NUMBER_BYTE_REFERENCE](AS_NUMBER_BYTE_REFERENCE.md).
