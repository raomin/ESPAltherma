# Detection data

Detection (`include/detect.h`) tries, in order:
1. a known fingerprint (`fingerprints.json`);
2. the indoor unit AS number (`as_numbers.json`), which gives the candidate definitions;
3. the survey rules, which choose among the candidates (or among all models when the AS number is unknown).

Both files are compiled into `include/catalog_data.h` by `python scripts/gen_catalog.py`. That script also runs before each build.

## as_numbers.json

The AS numbers of the known indoor unit boards, with the models they are fitted in and the matching definition files.

**How the AS number is read.** Registry 0x63 holds the printed AS number digits in order, one per nibble, at payload offsets 2–7. For example, AS1708171-30 F reads `01 70 81 71 03 06`.
- Checked on a real unit (EPGA16DAV3 + EAVH16S18DA6V): it returns AS1708171-30 F, the board of that indoor unit.
- **Offset 7:** its low nibble is the revision letter code, in the Daikin alphabet without I and O (6 = F). The same reading printed `F` with converter 214, which rules out the `[letter|digit]` layout.
- **Second suffix digit:** it is the high nibble of offset 6 or of offset 7. That unit's suffix is 30, so the two cannot be told apart yet. The other nibble carries the high bit of the letter code, which is 0 for A–Q, and the known boards only use A–H and M. So the non-zero nibble is the digit, whichever layout is right.
- The EEPROM image stores the same digits packed in a different order. This was checked on a real EEPROM dump.

**What the AS number cannot tell:**
- DA and DJ units share their AS numbers (ERGA D EHV/EHVZ, 38 numbers). The two definitions are in different families, so the survey rules separate them.
- A few ranges differ only by the second suffix digit: LT 04–08 vs 11–16 kW, EPRA E 8–12 kW tank vs no tank, and the 4–8 kW E monobloc vs the mini chiller. The table has entries for those full suffixes.
- EKHWET is given both `EKHWET-BAV3(Multi DHW tank)` and `Altherma(LT_Multi_DHWHP)`: its spare part is named "Multi DHWHP".

**Approximations** (no definition of their own; the closest one is used):
- the EBLA/EDLA 04–08 E monobloc → the D 4–8 kW definition;
- the Rotex RBLQ C2 → the LT 5–7 kW monobloc.

**Added from the field** (they carry a `note`):
- AS1706406, another board of the LT CA/CB indoor units, numbered like AS1706432. Suffix -29 is an EHVH08S18CB3V (seen on its display), and suffix -22 comes from a heat pump report: its outdoor unit is 14 kW, and it matches 1706432-22. The other suffixes of this board get both LT CA/CB definitions as candidates.

**Not in the table:**
- the outdoor unit board number in registry 0x11 (a 1Pxxxxxx board part number);
- the software ID in registry 0x60.

Both are reported, not used.

**Also used by the rules:** the outdoor capacity (registry 0x00 offset 12, kW ×10, e.g. 16 for an EPGA16), checked against the kW range in each definition name.

## fingerprints.json

Known heat pumps, for an exact detection when the AS number is not enough or not known. Each entry maps an identification key to a model (a definition file name of `definitions/`, without `.h`):

```json
[
  {"key": "I|63:017074500301|60:9482", "model": "Altherma(ERGA D EHV-EHB-EHVZ DJ series 04-08 kW)", "note": "reported by ..."}
]
```

- `key` is a **prefix** of the survey key: `"key"` in the report published on `espaltherma/detect`, also shown in Diagnostics in the web interface.
- The full key is `I|63:<registry 0x63 offsets 2-7>|60:<software id>|CAP:<capacity>|11:<outdoor board>|00:<outdoor MPU>`, in hex.
- A match gives a high confidence.

Keys come from users who share their detection report (the `espaltherma/detect` topic, or Diagnostics → Heat pump survey in the web interface) together with the model they confirmed: a confirmed model is the ground truth, useful for DA vs DJ.
