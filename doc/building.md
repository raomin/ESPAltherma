# Building ESPAltherma yourself

The [web installer](https://espaltherma.pages.dev/) is the easiest way, but you can build and upload the same firmware with [PlatformIO](https://platformio.org/):

```bash
git clone https://github.com/raomin/ESPAltherma.git
cd ESPAltherma
pio run -e web-m5stickcplus2 -t upload
```

Use the environment of your board:

| Board | Environment |
| ----- | ----------- |
| ESP32 DevKit | `web-esp32` |
| ESP32-C3 | `web-esp32c3` |
| ESP32-C6 | `web-esp32c6` |
| ESP32-S3 | `web-esp32s3` |
| M5StickC / M5StickC Plus / M5StickC Plus2 | `web-m5stickc` / `web-m5stickcplus` / `web-m5stickcplus2` |
| M5Stack Tough | `web-m5stack-tough` |
| M5Stack Atom Lite | `web-m5stack-atom` |
| WT32-ETH01 / Olimex ESP32-PoE | `web-wt32-eth01` / `web-esp32-poe` |

There is nothing to edit before building: the board starts its setup network (see [Step 1 of the README](../README.md#step-1-installing-the-firmware)) and everything is configured in the web interface. In VS Code, select the environment in the PlatformIO status bar and click Upload.

To update a board that is already installed, upload `.pio/build/<environment>/firmware.bin` in System → Firmware update, or upload over the network with `pio run -e <environment> -t upload --upload-port espaltherma.local` (with an admin password set, add `upload_flags = --auth=<your admin password>` to the environment in `platformio.ini`).

## Development builds

System → Firmware update offers the latest preview build when **Development builds** is ticked: it is the current state of the repository, less tested than a release.

## The definitions

The values of each heat pump model are described in the definition files of the [definitions](../definitions) folder, one file per series, with translated copies in the language sub-folders. The firmware's catalog of models is generated from them when building (`scripts/gen_catalog.py`), together with the AS number table (`data/as_numbers.json`) and the known identification keys (`data/fingerprints.json`). [How it works](how-it-works.md) explains how they are used.
