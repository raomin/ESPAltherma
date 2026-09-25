"""Builds the web flasher firmwares and their ESP Web Tools manifests.

    python scripts/build_flasher.py [--build] [--version X.Y.Z] [--out DIR]

For each board of flasher/boards.json:
  - --build runs `pio run -e <env>` (otherwise the existing build output is used),
  - copies .pio/build/<env>/firmware-factory.bin (merged image, flashed at 0) to <out>/firmware/<id>.bin,
    and firmware.bin (for updates from the web interface) to <out>/firmware/<id>-ota.bin,
  - writes <out>/manifest-<id>.json.

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
        build = os.path.join(ROOT, ".pio", "build", b["env"])
        factory = os.path.join(build, "firmware-factory.bin")
        if not os.path.exists(factory):
            missing.append(b["env"])
            continue
        shutil.copyfile(factory, os.path.join(firmware_dir, b["id"] + ".bin"))
        shutil.copyfile(os.path.join(build, "firmware.bin"), os.path.join(firmware_dir, b["id"] + "-ota.bin"))
        manifest = {
            "name": "ESPAltherma for " + b["name"],
            "version": version,
            "new_install_prompt_erase": True,
            "new_install_improv_wait_time": 20,
            "builds": [{"chipFamily": b["chip"], "parts": [{"path": "firmware/%s.bin" % b["id"], "offset": 0}]}],
        }
        with open(os.path.join(args.out, "manifest-%s.json" % b["id"]), "w", encoding="utf-8") as f:
            json.dump(manifest, f, indent=2)
        print("%-16s %7d bytes  -> firmware/%s.bin" % (b["id"], os.path.getsize(factory), b["id"]))

    if missing:
        print("Not built (run with --build): " + ", ".join(missing))
        sys.exit(1)


if __name__ == "__main__":
    main()
