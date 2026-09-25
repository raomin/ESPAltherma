"""PlatformIO post-build script: merges bootloader, partition table, boot_app0 and the application into one
image flashed at offset 0 (.pio/build/<env>/firmware-factory.bin). That is the file the web flasher installs.
"""

Import("env")  # noqa: F821


def quote(s):
    return '"%s"' % s if " " in s else s


def merge(source, target, env):
    board = env.BoardConfig()
    images = []
    for offset, image in env.get("FLASH_EXTRA_IMAGES", []):
        images += [offset, env.subst(image)]
    images += [env.subst("$ESP32_APP_OFFSET"), env.subst("$BUILD_DIR/${PROGNAME}.bin")]
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
