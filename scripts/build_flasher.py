"""Builds the web flasher firmwares and their ESP Web Tools manifests.

    python scripts/build_flasher.py [--build] [--version X.Y.Z] [--out DIR]

For each board of flasher/boards.json:
  - --build runs `pio run -e <env>` (otherwise the existing build output is used),
  - copies .pio/build/<env>/firmware-factory.bin (merged image, flashed at 0, for manual downloads) to
    <out>/firmware/<id>.bin, and firmware.bin (for updates from the web interface) to <out>/firmware/<id>-ota.bin,
  - copies the parts listed in flash-parts.json to <out>/firmware/<id>/ and writes <out>/manifest-<id>.json with
    them: written at their own offsets, they leave the NVS partition (the settings) alone, unless the user
    chooses to erase the device.

Local test: python scripts/build_flasher.py --build, then python scripts/serve_flasher.py and open
http://localhost:8000 in Chrome or Edge (Web Serial works on localhost).
"""

import argparse
import json
import os
import re
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def firmware_version():
    with open(os.path.join(ROOT, "include", "version.h"), encoding="utf-8") as f:
        m = re.search(r'#define ESPALTHERMA_VERSION "([^"]+)"', f.read())
    return m.group(1) if m else "dev"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--build", action="store_true", help="build the firmwares first")
    ap.add_argument("--version", default=None, help="version shown by the flasher (default: include/version.h)")
    ap.add_argument("--out", default=os.path.join(ROOT, "flasher"), help="output directory")
    args = ap.parse_args()

    version = args.version or firmware_version()
    with open(os.path.join(ROOT, "flasher", "boards.json"), encoding="utf-8") as f:
        boards = json.load(f)

    if args.build:
        cmd = ["pio", "run", "-d", ROOT]
        for b in boards:
            cmd += ["-e", b["env"]]
        env = dict(os.environ)
        if args.version:
            env["PLATFORMIO_BUILD_FLAGS"] = '-DESPALTHERMA_VERSION=\\"%s\\"' % args.version
        subprocess.check_call(cmd, env=env)

    firmware_dir = os.path.join(args.out, "firmware")
    os.makedirs(firmware_dir, exist_ok=True)
    missing = []
    for b in boards:
        # PLATFORMIO_BUILD_DIR: a build folder of its own, eg. while the IDE's PlatformIO uses .pio/build
        build = os.path.join(os.environ.get("PLATFORMIO_BUILD_DIR") or os.path.join(ROOT, ".pio", "build"), b["env"])
        factory = os.path.join(build, "firmware-factory.bin")
        if not os.path.exists(factory) or not os.path.exists(os.path.join(build, "flash-parts.json")):
            missing.append(b["env"])
            continue
        shutil.copyfile(factory, os.path.join(firmware_dir, b["id"] + ".bin"))
        shutil.copyfile(os.path.join(build, "firmware.bin"), os.path.join(firmware_dir, b["id"] + "-ota.bin"))
        with open(os.path.join(build, "flash-parts.json"), encoding="utf-8") as f:
            flash_parts = json.load(f)
        parts_dir = os.path.join(firmware_dir, b["id"])
        os.makedirs(parts_dir, exist_ok=True)
        parts = []
        for part in flash_parts:
            name = os.path.basename(part["path"])
            shutil.copyfile(part["path"], os.path.join(parts_dir, name))
            parts.append({"path": "firmware/%s/%s" % (b["id"], name), "offset": part["offset"]})
        manifest = {
            "name": "ESPAltherma for " + b["name"],
            "version": version,
            "new_install_prompt_erase": True,
            "new_install_improv_wait_time": 20,
            "builds": [{"chipFamily": b["chip"], "parts": parts}],
        }
        with open(os.path.join(args.out, "manifest-%s.json" % b["id"]), "w", encoding="utf-8") as f:
            json.dump(manifest, f, indent=2)
        print("%-16s %7d bytes  -> firmware/%s.bin" % (b["id"], os.path.getsize(factory), b["id"]))

    if missing:
        print("Not built (run with --build): " + ", ".join(missing))
        sys.exit(1)


if __name__ == "__main__":
    main()
