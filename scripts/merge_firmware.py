"""PlatformIO post-build script: merges bootloader, partition table, boot_app0 and the application into one
image flashed at offset 0 (.pio/build/<env>/firmware-factory.bin), for manual downloads.

It also lists those parts with their offsets (.pio/build/<env>/flash-parts.json): the web flasher writes them one
by one, so the NVS partition between them (the settings) survives a reinstall. The merged image, padded with
0xFF, would erase it.
"""

import json

Import("env")  # noqa: F821


def quote(s):
    return '"%s"' % s if " " in s else s


def merge(source, target, env):
    board = env.BoardConfig()
    images = []
    for offset, image in env.get("FLASH_EXTRA_IMAGES", []):
        images += [offset, env.subst(image)]
    images += [env.subst("$ESP32_APP_OFFSET"), env.subst("$BUILD_DIR/${PROGNAME}.bin")]
    parts = [{"offset": int(images[i], 0), "path": images[i + 1]} for i in range(0, len(images), 2)]
    with open(env.subst("$BUILD_DIR/flash-parts.json"), "w", encoding="utf-8") as f:
        json.dump(parts, f, indent=2)
    output = env.subst("$BUILD_DIR/firmware-factory.bin")
    cmd = [
        env.subst("$PYTHONEXE"), env.subst("$OBJCOPY"),
        "--chip", board.get("build.mcu", "esp32"),
        "merge_bin", "-o", output,
        "--flash_mode", "dio", "--flash_freq", "keep",
        "--flash_size", board.get("upload.flash_size", "4MB"),
    ] + images
    env.Execute(" ".join(quote(c) for c in cmd))


env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", merge)  # noqa: F821
