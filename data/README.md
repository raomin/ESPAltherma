# Detection data

Detection (`include/detect.h`) tries, in order:
1. a known fingerprint (`fingerprints.json`);
2. the indoor unit AS number (`as_numbers.json`), which gives the candidate definitions;
3. the survey rules, which choose among the candidates (or among all models when the AS number is unknown).

Both files are compiled into `include/catalog_data.h` by `python scripts/gen_catalog.py`. That script also runs before each build.

## as_numbers.json

The AS numbers of the known indoor unit boards, with the models they are fitted in and the matching definition files.

**How the AS number is read.** Registry 0x63 holds the printed AS number digits in order, two per byte (BCD, high nibble first), at payload offsets 2-7: AS1706432-27 A reads `01 70 64 32 27 01`.
- **Offset 6** is the suffix. A one-digit suffix reads with a 0 high nibble: `03` is "-3", a spare part board. Checked on 16 reported units (ESPAltherma 2 beta, October 2026): every reading matches a board of the table, with the outdoor unit's size and the owner's model.
- **Offset 7:** its low nibble is the revision letter code, in the Daikin alphabet without I and O (1 = A, 6 = F). Units may carry an older revision letter than the table's.
- A unit can carry the spare part image of its class rather than its model's: the EPGA16DAV3 + EAVH16S18DA6V that checked this reads AS1708171-3 F, the "EAV S18" spare part, with the model name stored separately. The first digit entry of the body (3x) still gives its definition.

**What the AS number cannot tell:**
- DA and DJ units share their AS numbers (ERGA D EHV/EHVZ, 38 numbers). The two definitions are in different families, so the survey rules separate them.
- A few ranges differ only by the second suffix digit: LT 04–08 vs 11–16 kW, EPRA E 8–12 kW tank vs no tank, and the 4–8 kW E monobloc vs the mini chiller. The table has entries for those full suffixes.
- EKHWET is given both `EKHWET-BAV3(Multi DHW tank)` and `Altherma(LT_Multi_DHWHP)`: its spare part is named "Multi DHWHP".

**Approximations** (no definition of their own; the closest one is used):
- the EBLA/EDLA 04–08 E monobloc → the D 4–8 kW definition;
- the Rotex RBLQ C2 → the LT 5–7 kW monobloc.

**Added from the field** (they carry a `note`):
- AS1706406: some Hybrid, GEO2 and LT CA/CB indoor units report this number with their own model suffix, numbered like the AS1706407 (Hybrid), AS1706408 (GEO2), AS1706409 (LT CA) and AS1706432 (LT CB) boards. Each suffix gets the definitions of those boards; where they share a suffix (10–12), the rules choose. Seen in the field: -29 on the display of an EHVH08S18CB3V, -22 from a 14 kW LT CB, and -03 from a replacement board. Spare part suffixes (2–7) get any of the board's definitions.

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
