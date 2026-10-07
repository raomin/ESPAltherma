"""Generates include/catalog_data.h, the label catalog of the firmware, from definitions/*.h.

The definition files stay the single source of truth. Every model (definition file) gets a bit;
identical entries of several models are merged into one catalog entry carrying the mask of those models.
The firmware builds the values of the detected model from the entries whose mask contains it.

Usage:
    python scripts/gen_catalog.py            # regenerate
    python scripts/gen_catalog.py --check    # exit 1 if include/catalog_data.h is stale (CI)

Also runs as a PlatformIO pre-build script (extra_scripts = pre:scripts/gen_catalog.py), in which case the
header is only rewritten when its content changes, so builds are not needlessly recompiled.
"""

import collections
import glob
import json
import os
import re
import sys

try:
    Import("env")  # noqa: F821 - defined when running as a PlatformIO extra script
    ROOT = env.subst("$PROJECT_DIR")  # noqa: F821
except NameError:
    ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

DEF_DIR = os.path.join(ROOT, "definitions")
OUTPUT = os.path.join(ROOT, "include", "catalog_data.h")
# Known identification keys (see survey.h surveyKey) -> model, for an exact detection
FINGERPRINTS = os.path.join(ROOT, "data", "fingerprints.json")
# Indoor unit AS numbers -> candidate definitions
AS_NUMBERS = os.path.join(ROOT, "data", "as_numbers.json")

# Not models: the user's own file, and DEFAULT.h which conflicts with every family.
EXCLUDED = {"mydef.h", "DEFAULT.h"}

# Family of each definition file.
#  G: Gen-2 (R32 D/E series): 0x20/12 is the high pressure, has 0xA0/0xA1
#  L: Gen-1 (LT, Hybrid, GEO2...): 0x20/12 is the heat sink temperature, no 0xA0
#  S: Protocol S, R: Protocol S (ROTEX)
FAMILIES = {
    "Altherma(EBLA-EDLA D series 4-8kW Monobloc).h": "G",
    "Altherma(EBLA-EDLA D series 9-16kW Monobloc).h": "G",
    "Altherma(EGSAH-X-EWSAH-X-D series 6-10kW GEO3).h": "G",
    "Altherma(EPGA D EAB-EAV-EAVZ D(J) series 11-16kW).h": "G",
    "Altherma(EPRA D ETSH-X 16P30-50 D series 14-16kW-ECH2O).h": "G",
    "Altherma(EPRA D ETV16-ETB16-ETVZ16 D series 14-16kW).h": "G",
    "Altherma(EPRA D_D7 ETSH-X 16P30-50 E_E7 series 14-18kW-ECH2O).h": "G",
    "Altherma(EPRA D_D7 ETV16-ETB16-ETVZ16 E_E7 series 14-18kW).h": "G",
    "Altherma(EPRA E ETSH-X 16P30-50 E series 8-12kW-ECH2O).h": "G",
    "Altherma(EPRA E ETV16-ETB16-ETVZ16 E_EJ series 8-12kW).h": "G",
    "Altherma(ERGA D EHV-EHB-EHVZ DJ series 04-08 kW).h": "G",
    "Altherma(ERGA E EHSH-X P30-50 E_EF series 04-08kW-ECH2O).h": "G",
    "Altherma(ERGA E EHV-EHB-EHVZ E_EJ series 04-08kW).h": "G",
    "Altherma(ERLA D EBSH-X 16P30-50 D SERIES 11-16kW-ECH2O).h": "G",
    "Altherma(ERLA D EBV-EBB-EBVZ D SERIES 11-16kW).h": "G",
    "Daikin Mini chiller(EWAA-EWYA D series 4-8kW).h": "G",
    "EKHWET-BAV3(Multi DHW tank).h": "G",
    "Altherma(EGSQH-A series 10kW GEO2).h": "L",
    "Altherma(ERGA D EHSH-X P30-50 D series 04-08kW-ECH2O).h": "L",
    "Altherma(ERGA D EHV-EHB-EHVZ DA series 04-08kW).h": "L",
    "Altherma(ERLA03 D EHFH-EHFZ DJ series 3kW).h": "L",
    "Altherma(Hybrid).h": "L",
    "Altherma(LT_CA_CB_04-08kW).h": "L",
    "Altherma(LT_CA_CB_11-16kW).h": "L",
    "Altherma(LT_CB_04-08kW Bizone).h": "L",
    "Altherma(LT_CB_11-16kW Bizone).h": "L",
    "Altherma(LT_EBLQ-EBLQ-CA series 5-7kW Monobloc).h": "L",
    "Altherma(LT_EBLQ-EDLQ-CA series 11-16kW Monobloc).h": "L",
    "Altherma(LT_Multi_DHWHP).h": "L",
    "Altherma(LT_Multi_Hybrid).h": "L",
    "Daikin Mini chiller(EWAA-EWYA D series 9-16kW).h": "L",
    "Daikin Mini chiller(EWAQ-EWYQ B series 4-8kW).h": "L",
    "PROTOCOL_S.h": "S",
    "PROTOCOL_S_ROTEX.h": "R",
}
FAMILY_ORDER = "GLSR"

# Values enabled by default on Protocol I models (exact label match). Protocol S models default to the
# lines their definition file ships uncommented.
RECOMMENDED = {
    "Operation Mode", "Thermostat ON/OFF", "Defrost Operation", "Error Code", "Error type",
    "R1T-Outdoor air temp.", "Outdoor air temp.",
    "INV primary current (A)", "INV frequency (rps)", "Voltage (N-phase) (V)",
    "Leaving water temp. before BUH (R1T)", "Leaving water temp. after BUH (R2T)",
    "Inlet water temp.(R4T)", "DHW tank temp. (R5T)", "Indoor ambient temp. (R1T)",
    "Refrig. Temp. liquid side (R3T)", "Discharge pipe temp.",
    "Flow sensor (l/min)", "Water pressure",
    "LW setpoint (main)", "DHW setpoint", "RT setpoint",
    "I/U operation mode", "3way valve(On:DHW_Off:Space)", "Water pump operation",
    "Circulation pump operation", "Powerful DHW Operation. ON/OFF", "Silent Mode",
    "Freeze Protection for water piping", "Brine inlet temp.", "Brine outlet temp.", "Mixed water temp.",
}
# Only recommended on the models whose definition file name contains the given word: other definitions carry
# these labels on slots that mean something else on the unit (eg. "Brine" temperatures on air units).
RECOMMENDED_ONLY = {"Brine inlet temp.": "GEO", "Brine outlet temp.": "GEO", "Mixed water temp.": "Bizone"}

# Capacity range in the definition name, eg. "04-08kW", "9-16kW", "3kW"
CAPACITY = re.compile(r"(\d+)(?:-(\d+))?\s*kW")

FLAG_RECOMMENDED = 1
FLAG_ALWAYS = 2  # refrigerant type: no data, selects the pressure->temperature conversion

LINE = re.compile(r'^\s*(//)?\s*\{\s*0x([0-9A-Fa-f]{2})\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*(-?\d+)\s*,\s*"((?:[^"\\]|\\.)*)"\s*\}')


# D-Checker layout markers and override commands found in the definition files: no value to read
NOT_VALUES = {995, 996, 998}


def parse(path):
    """Yields (reg, offset, conv, size, type, label, commented) for each label line."""
    with open(path, encoding="utf-8", errors="replace") as f:
        for line in f:
            m = LINE.match(line)
            if m and int(m.group(4)) not in NOT_VALUES:
                yield (int(m.group(2), 16), int(m.group(3)), int(m.group(4)), int(m.group(5)),
                       int(m.group(6)), m.group(7), m.group(1) is not None)


def c_string(s):
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'


# Sensor names in other languages, from the translated definition files (definitions/<Language>/). They are
# line-by-line translations of the English files: each English name gets the translation found most often.
# Only the names shown in Home Assistant and the web interface change; the MQTT keys and ids stay English.
LANGUAGES = [("fr", "French"), ("de", "German"), ("it", "Italian"), ("es", "Spanish")]


def translations(files, labels):
    out = ["// Sensor names by language: {English name offset in CATALOG_LABELS, name offset in the language pool},",
           "// sorted by the first. Names without a translation stay English."]
    table = []
    for code, folder in LANGUAGES:
        votes = collections.defaultdict(collections.Counter)
        for f in files:
            path = os.path.join(DEF_DIR, folder, f)
            if not os.path.exists(path):
                continue
            english = list(parse(os.path.join(DEF_DIR, f)))
            translated = list(parse(path))
            if [e[:5] for e in english] != [t[:5] for t in translated]:
                print("gen_catalog: %s/%s does not follow the English file, skipped" % (folder, f))
                continue
            for e, t in zip(english, translated):
                if t[5] != e[5] and e[5] in labels:
                    votes[e[5]][t[5]] += 1
        items = sorted((labels[en], c.most_common(1)[0][0]) for en, c in votes.items())
        pool, pos, pairs = [], 0, []
        offsets = {}
        for en_offset, text in items:
            if text not in offsets:
                offsets[text] = pos
                pool.append(text)
                pos += len(text.encode("utf-8")) + 1
            pairs.append((en_offset, offsets[text]))
        if pos > 0xFFFF:
            raise SystemExit("gen_catalog: %s name pool over 64KB" % code)
        upper = code.upper()
        out.append("static const char CATALOG_NAMES_%s[] =" % upper)
        for text in pool:
            out.append("    %s \"\\0\"" % c_string(text))
        out.append("    ;")
        out.append("static const CatalogTranslation CATALOG_TR_%s[] = {" % upper)
        for i in range(0, len(pairs), 8):
            out.append("    " + " ".join("{%d, %d}," % p for p in pairs[i:i + 8]))
        out.append("};")
        table.append('    {"%s", CATALOG_NAMES_%s, CATALOG_TR_%s, %d},' % (code, upper, upper, len(pairs)))
    out.append("#define CATALOG_LANGUAGE_COUNT %d" % len(LANGUAGES))
    out.append("static const CatalogLanguage CATALOG_LANGUAGES[CATALOG_LANGUAGE_COUNT] = {")
    out += table
    out.append("};")
    out.append("")
    return out


def build():
    files = sorted(os.path.basename(p) for p in glob.glob(os.path.join(DEF_DIR, "*.h")))
    files = [f for f in files if f not in EXCLUDED]
    unknown = [f for f in files if f not in FAMILIES]
    if unknown:
        raise SystemExit("gen_catalog: no family for %s - add it to FAMILIES in scripts/gen_catalog.py" % unknown)
    if len(files) > 64:
        raise SystemExit("gen_catalog: more than 64 models, the model mask needs to grow")

    models = []  # (name, family)
    entries = {}  # (family, reg, offset, conv, size, type, label, recommended) -> [models mask, flags, first seen]
    seen = 0
    for index, f in enumerate(files):
        family = FAMILIES[f]
        models.append((f[:-2], family))
        for reg, offset, conv, size, dtype, label, commented in parse(os.path.join(DEF_DIR, f)):
            if family in "SR":
                recommended = not commented
            else:
                recommended = label in RECOMMENDED and RECOMMENDED_ONLY.get(label, "") in f
            # An entry is split when it is recommended on some models only
            key = (family, reg, offset, conv, size, dtype, label, recommended)
            if key not in entries:
                entries[key] = [0, 0, seen]
                seen += 1
            e = entries[key]
            e[0] |= 1 << index
            if recommended:
                e[1] |= FLAG_RECOMMENDED
            if size == 0 and 800 <= conv <= 803:
                e[1] |= FLAG_ALWAYS

    # Registry, then offset order; definition file order within a slot (keeps the ON/OFF bits order)
    ordered = sorted(entries.items(), key=lambda kv: (FAMILY_ORDER.index(kv[0][0]), kv[0][1], kv[0][2], kv[1][2]))

    # Round trip: the entries of each model must be exactly the lines of its definition file.
    for index, f in enumerate(files):
        expected = {e[:6] for e in parse(os.path.join(DEF_DIR, f))}
        rebuilt = {k[1:7] for k, (mask, _, _) in ordered if mask >> index & 1}
        if expected != rebuilt:
            raise SystemExit("gen_catalog: round trip failed for %s" % f)

    labels = {}
    pool = []
    pos = 0
    for key, _ in ordered:
        label = key[6]
        if label not in labels:
            labels[label] = pos
            pool.append(label)
            pos += len(label.encode("utf-8")) + 1
    if pos > 0xFFFF:
        raise SystemExit("gen_catalog: label pool over 64KB")

    out = []
    out.append("// Generated by scripts/gen_catalog.py from definitions/*.h - DO NOT EDIT.")
    out.append("// %d models, %d entries, %d label bytes." % (len(models), len(ordered), pos))
    out.append("#pragma once")
    out.append("")
    out.append("#define CATALOG_MODEL_COUNT %d" % len(models))
    out.append("#define CATALOG_ENTRY_COUNT %d" % len(ordered))
    out.append("")
    out.append("// name, family, protocol, capacity range in kW from the name (0: not told)")
    out.append("static const CatalogModel CATALOG_MODELS[CATALOG_MODEL_COUNT] = {")
    for name, family in models:
        protocol = "S" if family in "SR" else "I"
        m = CAPACITY.search(name)
        low, high = (int(m.group(1)), int(m.group(2) or m.group(1))) if m else (0, 0)
        out.append("    {%s, '%s', '%s', %d, %d}," % (c_string(name), family, protocol, low, high))
    out.append("};")
    out.append("")
    out.append("static const char CATALOG_LABELS[] =")
    for label in pool:
        out.append("    %s \"\\0\"" % c_string(label))
    out.append("    ;")
    out.append("")
    out.append("// reg, offset, conv, size, type, label, flags, models")
    out.append("static const CatalogEntry CATALOG_ENTRIES[CATALOG_ENTRY_COUNT] = {")
    for (family, reg, offset, conv, size, dtype, label, _), (mask, flags, _) in ordered:
        out.append("    {0x%02x, %d, %d, %d, %d, %d, %d, 0x%016xULL}," % (reg, offset, conv, size, dtype, labels[label], flags, mask))
    out.append("};")
    out.append("")
    out += translations(files, labels)

    fingerprints = []
    if os.path.exists(FINGERPRINTS):
        with open(FINGERPRINTS, encoding="utf-8") as f:
            fingerprints = json.load(f)
    names = [name for name, _ in models]
    out.append("// Known identification keys (data/fingerprints.json): a survey key starting with key is that model")
    out.append("#define CATALOG_FINGERPRINT_COUNT %d" % len(fingerprints))
    out.append("static const CatalogFingerprint CATALOG_FINGERPRINTS[CATALOG_FINGERPRINT_COUNT + 1] = {")
    for fp in fingerprints:
        if fp["model"] not in names:
            raise SystemExit("gen_catalog: unknown model in fingerprints.json: %s" % fp["model"])
        out.append("    {%s, %d}," % (c_string(fp["key"]), names.index(fp["model"])))
    out.append("    {nullptr, -1},")
    out.append("};")
    out.append("")

    # AS numbers: candidates of the whole body (spare part boards, unknown suffixes), of (body, first suffix
    # digit), and of the full suffix when it narrows them down.
    as_masks = {}
    source = ""
    if os.path.exists(AS_NUMBERS):
        with open(AS_NUMBERS, encoding="utf-8") as f:
            data = json.load(f)
        source = data.get("source", "")
        for e in data["as_numbers"]:
            mask = 0
            for d in e["defs"]:
                if d not in names:
                    raise SystemExit("gen_catalog: unknown definition in as_numbers.json: %s" % d)
                mask |= 1 << names.index(d)
            if not mask:
                continue
            body = int(e["body"])
            suffix = e["suffix"]
            keys = [(body, 0xFF, 0xFF), (body, int(suffix[0]), 0xFF)]
            if len(suffix) == 2:
                keys.append((body, int(suffix[0]), int(suffix[1])))
            for key in keys:
                as_masks[key] = as_masks.get(key, 0) | mask
        # Full suffixes are only kept when they tell more than the first digit
        as_masks = {k: v for k, v in as_masks.items() if k[2] == 0xFF or v != as_masks[(k[0], k[1], 0xFF)]}
    out.append("// Indoor unit AS numbers (data/as_numbers.json, %s): body, suffix digits (0xFF = any), candidates" % source)
    out.append("#define CATALOG_AS_COUNT %d" % len(as_masks))
    out.append("static const CatalogAsNumber CATALOG_AS[CATALOG_AS_COUNT + 1] = {")
    for (body, s1, s2), mask in sorted(as_masks.items()):
        out.append("    {%d, 0x%02x, 0x%02x, 0x%016xULL}," % (body, s1, s2, mask))
    out.append("    {0, 0, 0, 0},")
    out.append("};")
    out.append("")
    return "\n".join(out)


def main():
    content = build()
    current = None
    if os.path.exists(OUTPUT):
        with open(OUTPUT, encoding="utf-8") as f:
            current = f.read()
    if "--check" in sys.argv:
        if current != content:
            print("include/catalog_data.h is stale: run python scripts/gen_catalog.py")
            sys.exit(1)
        return
    if current != content:
        with open(OUTPUT, "w", encoding="utf-8", newline="\n") as f:
            f.write(content)
        print("gen_catalog: include/catalog_data.h updated")


main()
