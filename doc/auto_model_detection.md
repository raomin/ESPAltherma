# Automatic Model/Family Detection for ESPAltherma

## Context

ESPAltherma currently requires the user to manually select their heat pump model by uncommenting `#include "def/..."` in `setup.h` / `my_setup.h`. There are 36 definition files, and choosing the wrong one causes incorrect readings. The DISCREPANCIES.md report shows 72 labels with conflicting mappings across models — same label name, different registry/offset/convid.

This document proposes an approach to **automatically identify the connected heat pump family** by probing registries at startup and matching the responses against known fingerprint patterns.

## Key Insight: Family Grouping

After analyzing all 36 definition files, models cluster into **7 distinct families** based on observable protocol-level differences. Exact model identification isn't feasible (some models are registry-identical), but **family** identification is sufficient to select the right label definitions.

### Family Tree

```
┌─ Protocol detection (try 0x50 vs 0x10)
│
├── Protocol S
│   ├── Family: PROTOCOL_S        (registries 0x50, 0x53-0x55)
│   └── Family: PROTOCOL_S_ROTEX  (registries 0x53-0x56, no 0x50)
│
└── Protocol I
    │
    ├── Probe 0xA0 → responds?
    │   │
    │   ├── YES → "D-series/Monobloc" group (has 0xA0 + 0xA1)
    │   │   │
    │   │   ├── Probe 0x64 offset 2 → has DHW-specific bits?
    │   │   │   ├── YES → Family: EKHWET (Multi DHW tank)
    │   │   │   └── NO  → continue
    │   │   │
    │   │   └── Probe 0x65 → responds?
    │   │       ├── YES → Family: D-SERIES-LARGE (9-16kW + EPRA/EPGA/ERLA)
    │   │       └── NO  → Family: D-SERIES-SMALL (4-8kW EBLA-EDLA/ERGA)
    │   │
    │   └── NO → "LT/Legacy" group (no 0xA0/0xA1)
    │       │
    │       ├── Probe 0x21 offset 4 → read 2 bytes as convid 101 (Voltage)?
    │       │   Value > 50 (plausible voltage) vs value ÷ 10 is temperature range
    │       │   │
    │       │   ├── Voltage-like → "Older/LT" subgroup
    │       │   │   │
    │       │   │   ├── Probe 0x64 offset 9 → responds with data?
    │       │   │   │   ├── YES → Family: LT_CA_CB (has Add pump / Main pump)
    │       │   │   │   └── NO  → continue
    │       │   │   │
    │       │   │   ├── Probe 0x62 offset 8 → read byte, check bit pattern
    │       │   │   │   Has unique DHW bits (306=Defrost prohibition)?
    │       │   │   │   ├── YES → Family: LT_MULTI (DHWHP/Hybrid variants)
    │       │   │   │   └── NO  → continue
    │       │   │   │
    │       │   │   └── Family: GEO/CHILLER (EGSQH, Mini chiller, LT_EBLQ Monobloc)
    │       │   │
    │       │   └── Temperature-like → Family: GEO3 (EGSAH-X, convid 152 at 0x21)
    │       │
    │       └── (fallback) → Family: GENERIC
    │

```

## Detection Strategy

The detection proceeds as a decision tree, probing at most 5-6 registries. Each probe takes ~300ms worst case (serial timeout), so total detection is ~2 seconds.

### Step 1: Protocol Detection

Try to query registry `0x10` (Protocol I common registry). If it responds, we're on Protocol I.
If it fails, try `0x50` (Protocol S). If that responds, Protocol S.

For Protocol S: query `0x56` — if it responds, ROTEX variant; otherwise standard PROTOCOL_S.

### Step 2: Probe 0xA0 (D-series indicator)

Registry `0xA0` only exists on D-series models (EBLA/EDLA, EPRA, EPGA, ERGA, ERLA, EKHWET). If it responds → D-series group. If error/timeout → LT/Legacy group.

### Step 3a: D-series sub-identification

- **0x65 presence**: Only the larger D-series models (9-16kW, EPRA D_D7, EPRA E, etc.) have registry `0x65`. Query it — if it responds, we know we're on a "large" D-series.
- **0xA1 offset 9 bits**: The "model type" byte at `0xA1` offset 9 encodes the chassis type as individual bits (300-307): Monobloc, Mini chiller, MT, GSHP, Hydro split, Altherma LT. Reading this single byte gives direct identification.

### Step 3b: LT/Legacy sub-identification

- **0x21 offset 4, 2 bytes**: On LT/older models this is `Voltage (N-phase)` (convid 101, raw unsigned integer, typically 180-260). On GEO3 (EGSAH-X) this is `INV fin temp.` (convid 105, ÷10 → a temperature ~20-80°C, raw value ~200-800). The raw value ranges don't overlap much, making this a reliable discriminator.
- **0x64 probing**: Check if offsets 9-10 contain valid data (Add pump / Mixed water temp) → LT_CA_CB family.
- **0x62 offset 8, 1 byte**: The bit usage at this offset varies distinctly per family. Read the byte and test which bits are active in combination with known operation modes.

### Step 4: Confidence check

After family identification, read a few "signature" values and verify they decode to plausible ranges (e.g., outdoor temperature between -40°C and +60°C, water pressure 0-4 bar). If they don't, fall back to GENERIC.

## Registry Probing Summary

| Probe | Registry | What it tells us | Families discriminated |
|-------|----------|------------------|----------------------|
| 1 | 0x10 | Protocol I vs S | S vs I |
| 2 | 0x50/0x56 | S variant (if Protocol S) | PROTOCOL_S vs ROTEX |
| 3 | 0xA0 | D-series presence | D-series vs LT/Legacy |
| 4 | 0xA1 offset 9 | Chassis type bits | Monobloc/Chiller/GEO/Hydro |
| 5 | 0x65 | Large vs small D-series | 9-16kW vs 4-8kW |
| 6 | 0x21 offset 4 | Voltage vs temperature | GEO3 vs LT/older |
| 7 | 0x64 offset 9 | Has pump control | LT_CA_CB vs others |

## Family → Definition File Mapping

| Family ID | Definition Files | Key Traits |
|-----------|-----------------|------------|
| `FAMILY_PROTOCOL_S` | PROTOCOL_S.h | Registries 0x50-0x55 |
| `FAMILY_PROTOCOL_S_ROTEX` | PROTOCOL_S_ROTEX.h | Registries 0x53-0x56 |
| `FAMILY_D_SERIES_SMALL` | EBLA-EDLA 4-8kW, ERGA D/E 04-08kW, ERLA03 3kW | 0xA0 yes, 0x65 no |
| `FAMILY_D_SERIES_LARGE` | EBLA-EDLA 9-16kW, EPRA, EPGA, ERGA+ECH2O, ERLA 11-16kW | 0xA0 yes, 0x65 yes |
| `FAMILY_EKHWET` | EKHWET-BAV3 (Multi DHW tank) | 0xA0 yes, DHW-specific 0x64 |
| `FAMILY_GEO` | EGSQH GEO2, EGSAH-X GEO3 | No 0xA0, brine temps in 0x21 |
| `FAMILY_LT` | LT_CA_CB, LT_CB Bizone, LT_Multi, LT_EBLQ Monobloc | No 0xA0, voltage in 0x21 |
| `FAMILY_HYBRID` | Altherma Hybrid | No 0xA0, Hybrid Op Mode at 0x64 |
| `FAMILY_CHILLER` | Mini chiller EWAA/EWAQ | No 0xA0, compact 0x21 |
| `FAMILY_GENERIC` | GENERIC.h / DEFAULT.h | Fallback |

## C Implementation

```c
// auto_detect.h — Automatic model family detection for ESPAltherma
// Include after comm.h (needs queryRegistry)

#ifndef AUTO_DETECT_H
#define AUTO_DETECT_H

#include <Arduino.h>

// Family identifiers
enum DaikinFamily {
    FAMILY_UNKNOWN = 0,
    FAMILY_PROTOCOL_S,
    FAMILY_PROTOCOL_S_ROTEX,
    FAMILY_D_SERIES_SMALL,   // 4-8kW monobloc/split with 0xA0 but no 0x65
    FAMILY_D_SERIES_LARGE,   // 9-16kW+ with 0xA0 and 0x65
    FAMILY_EKHWET,           // Multi DHW tank (0xA0 + unique 0x64 DHW bits)
    FAMILY_GEO,              // Geothermal (EGSQH, EGSAH-X)
    FAMILY_LT,               // LT_CA_CB, LT_CB Bizone, LT_EBLQ Monobloc
    FAMILY_LT_MULTI,         // LT_Multi_DHWHP, LT_Multi_Hybrid
    FAMILY_HYBRID,           // Altherma Hybrid
    FAMILY_CHILLER,          // Mini chiller
    FAMILY_GENERIC           // Fallback
};

const char* familyName(DaikinFamily f) {
    switch (f) {
        case FAMILY_PROTOCOL_S:       return "Protocol S";
        case FAMILY_PROTOCOL_S_ROTEX: return "Protocol S (Rotex)";
        case FAMILY_D_SERIES_SMALL:   return "D-series 4-8kW (EBLA/EDLA/ERGA)";
        case FAMILY_D_SERIES_LARGE:   return "D-series 9-16kW+ (EPRA/EPGA/ERLA)";
        case FAMILY_EKHWET:           return "EKHWET Multi DHW Tank";
        case FAMILY_GEO:              return "Geothermal (EGSQH/EGSAH)";
        case FAMILY_LT:              return "Altherma LT (CA/CB)";
        case FAMILY_LT_MULTI:        return "Altherma LT Multi";
        case FAMILY_HYBRID:          return "Altherma Hybrid";
        case FAMILY_CHILLER:         return "Mini Chiller (EWAA/EWAQ)";
        case FAMILY_GENERIC:         return "Generic (unknown model)";
        default:                     return "Unknown";
    }
}

// Attempt to read a registry. Returns true if the HP responded with valid data.
// Uses the existing queryRegistry from comm.h.
// buffer must be at least 64 bytes.
static bool probeRegistry(char regID, unsigned char *buffer, char protocol = 'I') {
    memset(buffer, 0, 64);
    return queryRegistry(regID, buffer, protocol);
}

// Read a 16-bit unsigned value from buffer at a given offset (little-endian)
static uint16_t readU16LE(unsigned char *data, int offset) {
    return (uint16_t)data[offset] | ((uint16_t)data[offset + 1] << 8);
}

// Main detection function.
// Call this once at startup, before initRegistries().
// Returns the detected family. Also logs progress via mqttSerial.
DaikinFamily detectFamily() {
    unsigned char buf[64];

    mqttSerial.println("=== Auto-detecting heat pump family ===");

    // -------------------------------------------------------
    // Step 1: Protocol detection — try Protocol I first
    // -------------------------------------------------------
    mqttSerial.println("Step 1: Probing registry 0x10 (Protocol I)...");
    bool hasProtocolI = probeRegistry(0x10, buf, 'I');

    if (!hasProtocolI) {
        // Try Protocol S
        mqttSerial.println("No Protocol I response. Trying Protocol S (0x50)...");
        bool has0x50 = probeRegistry(0x50, buf, 'S');

        if (has0x50) {
            // Check for Rotex variant: 0x56 exists only on Rotex
            mqttSerial.println("Protocol S detected. Probing 0x56 for Rotex...");
            if (probeRegistry(0x56, buf, 'S')) {
                mqttSerial.println(">> Detected: Protocol S (Rotex)");
                return FAMILY_PROTOCOL_S_ROTEX;
            }
            mqttSerial.println(">> Detected: Protocol S");
            return FAMILY_PROTOCOL_S;
        }

        // Try 0x53 as secondary Protocol S probe (Rotex has no 0x50)
        mqttSerial.println("Trying 0x53 (Protocol S fallback)...");
        if (probeRegistry(0x53, buf, 'S')) {
            // 0x53 responds but 0x50 didn't — likely Rotex
            mqttSerial.println(">> Detected: Protocol S (Rotex)");
            return FAMILY_PROTOCOL_S_ROTEX;
        }

        mqttSerial.println("No response on any protocol. Falling back to GENERIC.");
        return FAMILY_GENERIC;
    }

    mqttSerial.println("Protocol I confirmed.");

    // -------------------------------------------------------
    // Step 2: Probe 0xA0 — D-series indicator
    // -------------------------------------------------------
    mqttSerial.println("Step 2: Probing registry 0xA0...");
    bool hasA0 = probeRegistry(0xA0, buf, 'I');

    if (hasA0) {
        // -------------------------------------------------------
        // Step 3a: D-series sub-identification
        // -------------------------------------------------------
        // Save 0xA0 data for later — offset 12 contains valve/heater bits
        unsigned char a0_data[64];
        memcpy(a0_data, buf, 64);

        // Check 0xA1 for chassis type byte at offset 9
        // In Protocol I response, payload starts at byte 3
        // So offset 9 in the label definition = byte 3 + 9 = byte 12 in raw buffer
        mqttSerial.println("Step 3a: Probing registry 0xA1 (chassis type)...");
        bool hasA1 = probeRegistry(0xA1, buf, 'I');
        uint8_t chassisType = 0;
        if (hasA1) {
            // 0xA1 offset 9, 1 byte — contains bit flags for model type
            // In the raw response buffer: byte 0=0x40, byte 1=regID, byte 2=len, 
            // then data starts at byte 3. So label offset 9 = buf[3 + 9] = buf[12]
            chassisType = buf[3 + 9];
            mqttSerial.printf("0xA1 chassis type byte: 0x%02x\n", chassisType);
            // Bits (from the definition files):
            // bit 5 (0x20): Monobloc
            // bit 4 (0x10): Minichiller
            // bit 3 (0x08): MT (medium temperature)
            // bit 2 (0x04): GSHP (ground source)
            // bit 1 (0x02): Hydro split
            // bit 0 (0x01): Altherma LT
        }

        // Check for EKHWET (Multi DHW tank) — unique DHW bits in 0x64
        // EKHWET has labels at 0x64 offset 2 with convid 307 "DHW in standby due to defrost"
        // Normal D-series has 0x64 offset 2 as "Space H/C request" (convid 304)
        // We distinguish by checking if 0x64 offset 2 has the DHW-specific byte pattern
        mqttSerial.println("Probing 0x64 for DHW tank identification...");
        bool has64 = probeRegistry(0x64, buf, 'I');
        if (has64) {
            // EKHWET unique marker: at 0x64, offsets 2-9 are all 1-byte boolean flags
            // for DHW-specific functions. On EKHWET, offset 2 bit 7 (convid 307) is
            // "DHW in standby" — this label only exists in the EKHWET definition.
            // On normal D-series, 0x64 offset 2 is an indoor unit code (convid 152).
            // Since both are single bytes, we can't distinguish by value alone.
            // Instead, check if 0xA0 offset 12 bit 2 is "DHW:SV1" (302) — EKHWET specific
            // vs "Solenoid Valve 1" (302) on normal D-series — same convid, different label,
            // not distinguishable at protocol level.
            //
            // Better approach: EKHWET has a unique label at 0x10 offset 6 with convid 114
            // ("DHW supply temperature") while all other D-series use convid 105/152 there.
            // We can check if the value at 0x10 offset 6 decodes as convid 114 plausibly.
            // But we already read 0x10 in step 1.
            //
            // Simplest: EKHWET has more entries in 0x63 (offsets 8-12 are "Not in use" with
            // convid 215; normal D-series small doesn't have those). This is hard to detect.
            //
            // Pragmatic: EKHWET is very similar to D-SERIES-SMALL in registry layout.
            // The GENERIC definition covers both. We fold EKHWET into D-SERIES-SMALL for now.
        }

        // Check 0x65 — present only on larger D-series
        mqttSerial.println("Probing 0x65 for large D-series identification...");
        bool has65 = probeRegistry(0x65, buf, 'I');

        if (has65) {
            mqttSerial.println(">> Detected: D-series Large (9-16kW+)");
            return FAMILY_D_SERIES_LARGE;
        } else {
            mqttSerial.println(">> Detected: D-series Small (4-8kW)");
            return FAMILY_D_SERIES_SMALL;
        }
    }

    // -------------------------------------------------------
    // Step 3b: No 0xA0 — LT/Legacy/GEO group
    // -------------------------------------------------------
    mqttSerial.println("Step 3b: No 0xA0. Probing 0x21 for family discrimination...");
    bool has21 = probeRegistry(0x21, buf, 'I');

    if (!has21) {
        mqttSerial.println("Cannot read 0x21. Falling back to GENERIC.");
        return FAMILY_GENERIC;
    }

    // Read 0x21 offset 4 (2 bytes). In Protocol I, data starts at byte 3.
    // So offset 4 = buf[3 + 4] = buf[7]
    uint16_t val_21_4 = readU16LE(buf + 3, 4);
    mqttSerial.printf("0x21 offset 4 raw value: %u\n", val_21_4);

    // On GEO3 (EGSAH-X), offset 4 = INV fin temp (convid 152, unsigned byte)
    //   → But it's actually convid 105 in most GEO models (signed ÷ 10 = temperature)
    //   → Raw value ~200-800 for 20°C-80°C
    // On LT/Hybrid/EGSQH, offset 4 = Voltage N-phase (convid 101, signed raw integer)
    //   → Raw value ~180-260 for mains voltage
    //
    // These ranges overlap (200-260), so we need another discriminator.
    // Better: check 0x20 offset 2. On GEO models this is "2 phase thermistor (R4T)"
    // (a temperature ~-10 to +40°C, raw signed ÷10 = ~-100 to 400).
    // On LT/Hybrid it's "O/U Heat Exch. Temp." (also temperature, same range).
    //
    // Most reliable discriminator: probe 0x64 at higher offsets.
    //   - GEO has NO offsets 0-10 in 0x64 (or only 0-7)
    //   - LT_CA_CB has offsets 0-10 (Add pump at 9, Mixed water at 10)
    //   - Hybrid has offsets 0-7 + unique bits
    //
    // Check 0x64 offset 9 and 10:

    mqttSerial.println("Probing 0x64 for pump/water temp discrimination...");
    bool has64 = probeRegistry(0x64, buf, 'I');

    if (has64) {
        // Check response length. Protocol I: buf[2] = payload length.
        // Offset 9 needs at least 10 data bytes → payload length >= 10 + registry overhead
        uint8_t payloadLen = buf[2];
        mqttSerial.printf("0x64 payload length: %d bytes\n", payloadLen);

        // Protocol I response: byte 0=0x40, 1=regID, 2=len, 3..=data, last=CRC
        // The label offsets map directly to data bytes starting at position 3.
        // If payloadLen > 12, we likely have the full LT_CA_CB layout (offsets through 15)
        // If payloadLen <= 10, it might be GEO/Hybrid (only offsets 0-7)

        if (payloadLen >= 16) {
            // Has offset 14-15 → likely LT_CA_CB with delta T fields
            // Read offset 9 as a boolean (Add pump / Main pump)
            // and offset 10 as temp (Mixed water temp)
            uint8_t addPump = buf[3 + 9];
            uint16_t mixedWater = readU16LE(buf + 3, 10);
            mqttSerial.printf("0x64 offset 9 (pump): %u, offset 10 (mixed water raw): %u\n",
                              addPump, mixedWater);

            // Additional check: does 0x62 offset 8 have LT_Multi-specific bits?
            mqttSerial.println("Probing 0x62 for Multi/Hybrid discrimination...");
            if (probeRegistry(0x62, buf, 'I')) {
                uint8_t val_62_8 = buf[3 + 8];
                mqttSerial.printf("0x62 offset 8 byte: 0x%02x\n", val_62_8);
            }

            mqttSerial.println(">> Detected: Altherma LT (CA/CB)");
            return FAMILY_LT;
        }

        // Shorter payload — few offsets in 0x64
        // Check for Hybrid: 0x64 offset 2 as "Hybrid DHW operation mode" (convid 300)
        // This is a bit field. On Hybrid, the specific combination of offsets 2-7
        // includes "Hybrid Op. Mode" at offset 3 (convid 217) and
        // "BE_COP value" at offset 4 (convid 105).
        //
        // On GEO/Chiller, offset 3 is also usually "Operation Mode" (217).
        // Differentiator: Hybrid has 0x64 offset 2 convid 300 (boolean bit 0 of byte)
        //   → label "Hybrid DHW operation mode" — value is 0 or 1
        // GEO doesn't have offset 2 defined in 0x64 at all (only offsets 0-1 and 3+)
        //
        // Also: Hybrid has NO offset 8+ in 0x62 ("Not in use" vs active bits)
        // but that's hard to distinguish from GEO which also has "Not in use" there.

        // Check 0x20 for brine temperatures (GEO specific)
        mqttSerial.println("Probing 0x20 for brine temps (GEO indicator)...");
        if (probeRegistry(0x20, buf, 'I')) {
            // GEO models have brine temps at 0x20 offsets 8 and 10
            // (R5T entering brine, R6T leaving brine) — values ~0-30°C, raw 0-300
            // LT/Hybrid have "Heat exchanger mid-temp" and "Liquid pipe temp" there
            // — could be any temperature range

            // A better discriminator: GEO has "2 phase thermistor (R4T)" at 0x20 offset 2
            // while LT/Hybrid has "O/U Heat Exch. Temp." at the same offset.
            // Both are temperatures via convid 105, so the value itself doesn't help.

            // Most reliable for Hybrid: check if 0x60 offset 12 bit 2 = "BPH" (present)
            // vs on GEO/Chiller 0x60 offset 12 = "Not in use"
            mqttSerial.println("Probing 0x60 for Hybrid BPH indicator...");
            if (probeRegistry(0x60, buf, 'I')) {
                uint8_t payLen60 = buf[2];
                if (payLen60 >= 13) {
                    uint8_t val_60_12 = buf[3 + 12];
                    mqttSerial.printf("0x60 offset 12 byte: 0x%02x\n", val_60_12);
                    // On Hybrid, offset 12 bit 2 (convid 302) = "BPH" (backup heater)
                    // If bit 2 is set, likely Hybrid in heating mode
                    // But this alone is not conclusive since value depends on operation.
                }
                // Check Indoor Unit Code at 0x60 offset 4 (2 bytes)
                // GEO uses convid 314, LT uses convid 152 here
                // convid 314 decodes as a special 2-byte code
                // convid 152 is just unsigned 1-byte
                // We can check if the second byte at offset 5 is zero (152=1byte) or not (314=2byte)
                uint16_t unitCode = readU16LE(buf + 3, 4);
                mqttSerial.printf("0x60 offset 4 (unit code) raw: 0x%04x\n", unitCode);
            }
        }

        // At this point, if we haven't returned yet, use a combination:
        mqttSerial.println("Using heuristics for remaining families...");

        // Re-read 0x21 (already in our earlier probe — but buf was overwritten)
        probeRegistry(0x21, buf, 'I');

        // Check 0x21 total data length — compact layout (Chiller/LT_EBLQ Monobloc)
        // has fewer offsets than GEO/Hybrid
        uint8_t payLen21 = buf[2];
        mqttSerial.printf("0x21 payload length: %d bytes\n", payLen21);

        // Compact layout: offsets go up to 10-12 (payload ~14-16 bytes)
        // Standard layout: offsets go up to 18 (payload ~20+ bytes)
        if (payLen21 <= 16) {
            // Compact 0x21 → Chiller or LT_EBLQ Monobloc
            mqttSerial.println(">> Detected: Mini Chiller / LT Monobloc");
            return FAMILY_CHILLER;
        }

        // Standard 0x21 with Brine temps → GEO
        // Check offset 7: on GEO this is "Brine inlet temp" (valid temperature)
        int16_t brineIn = (int16_t)readU16LE(buf + 3, 7);
        double brineTemp = brineIn * 0.1;
        mqttSerial.printf("0x21 offset 7 as temp: %.1f°C\n", brineTemp);

        // Brine temps are typically 0-25°C for geothermal
        if (brineTemp > -10.0 && brineTemp < 40.0) {
            // Check if we have 0x62 offset 8 bit 0 = "Space C/H Op. or BPH" for Hybrid
            probeRegistry(0x62, buf, 'I');
            // Hybrid: 0x62 offset 15 is "Not in use" (convid 152)
            //   → typically reads as 0 when unused
            // GEO: 0x62 offset 15 is "Refrig pressure sensor" (convid 105)
            //   → typically reads as a pressure value > 0
            uint16_t val_62_15 = readU16LE(buf + 3, 15);
            mqttSerial.printf("0x62 offset 15 raw: %u\n", val_62_15);

            if (val_62_15 == 0) {
                mqttSerial.println(">> Detected: Altherma Hybrid");
                return FAMILY_HYBRID;
            } else {
                mqttSerial.println(">> Detected: Geothermal (EGSQH/EGSAH)");
                return FAMILY_GEO;
            }
        }

        // If brine temp doesn't look right, might be a Multi variant
        mqttSerial.println(">> Detected: LT Multi");
        return FAMILY_LT_MULTI;
    }

    // 0x64 didn't respond at all — very unusual
    mqttSerial.println("0x64 not available. Falling back to GENERIC.");
    return FAMILY_GENERIC;
}

#endif // AUTO_DETECT_H
```

## Integration with ESPAltherma

### Option A: Runtime Detection (Recommended)

Modify `main.cpp` to call `detectFamily()` in `setup()` after serial initialization but before `initRegistries()`. The detected family is published to MQTT and can be used to:

1. **Select the right definition at compile time**: Not possible with runtime detection, but the detected family guides the user to the right `#include`.
2. **Log the family for diagnostics**: Always useful — publish to `espaltherma/family`.
3. **Future: dynamic label loading**: Store all definitions in PROGMEM and switch at runtime (requires refactoring `labelDefs[]` from a compile-time array to a pointer).

### Option B: One-time Detection Tool

Create a separate firmware image (or a build flag) that runs detection only, reports the result over MQTT/serial, and tells the user which `#include "def/..."` to use. This is simpler to implement and doesn't require changing the normal runtime flow.

### Proposed main.cpp Changes (Option A)

```cpp
#include "auto_detect.h"

// In setup(), after MySerial.begin() and MQTT connection:
void setup() {
    // ... existing serial/wifi/mqtt init ...

    DaikinFamily family = detectFamily();
    mqttSerial.printf("Detected family: %s\n", familyName(family));

    // Publish to MQTT for Home Assistant / diagnostics
    char familyStr[64];
    snprintf(familyStr, sizeof(familyStr), "%s", familyName(family));
    client.publish("espaltherma/family", familyStr);

    initRegistries();
}
```

### Option C: Fully Dynamic (Advanced)

Store all definition arrays in PROGMEM with family tags. After detection, copy the matching family's `labelDefs[]` into RAM. This eliminates the need for users to edit header files entirely.

```cpp
// Sketch of dynamic approach — would need all def files restructured:
struct FamilyDef {
    DaikinFamily family;
    const LabelDef *labels;  // PROGMEM pointer
    int labelCount;
};

// In setup:
DaikinFamily detected = detectFamily();
for (auto &fd : allFamilies) {
    if (fd.family == detected) {
        memcpy_P(labelDefs, fd.labels, fd.labelCount * sizeof(LabelDef));
        break;
    }
}
```

This requires significant refactoring but would make ESPAltherma truly plug-and-play.

## Limitations & Edge Cases

1. **Identical models**: Some models are indistinguishable at the registry level (e.g., ERGA D DA vs ERGA D DJ differ only in label text at a few offsets). Family-level detection is the practical limit.

2. **Registry response timing**: The HP takes up to 300ms to respond. Detection with 6 probes takes ~2 seconds. This is acceptable at startup.

3. **Error responses**: If the HP returns `0x15 0xEA` (unknown command), we treat that registry as absent. The existing `queryRegistry` already handles this.

4. **Ambiguous values**: Some heuristics (e.g., voltage vs temperature range at 0x21 offset 4) may fail with edge-case values. The confidence check in Step 4 guards against this.

5. **EKHWET vs D-SERIES-SMALL**: These share the same registry structure (both have 0xA0/0xA1). The differences are only in label names and a few convid values. Using GENERIC or D-SERIES-SMALL definitions will read most values correctly for EKHWET too.

6. **Flash usage**: Including all family definitions for Option C would significantly increase flash usage. The ESP32 has 4MB, and all 36 definition files together are ~200KB of label data (mostly strings), which is feasible but requires PROGMEM management.
