# Adds a `merged` target to an ESP32 env: one image holding the bootloader, slot
# layout, Rrptr and the OTA updater, to flash at address 0 without PlatformIO.
#
#   pio run -e <env> -t merged
#
# A `merged_upload` target builds the same image and flashes it over USB, for a
# one-click first install or full reflash (PlatformIO's Project Tasks, Custom):
#
#   pio run -e <env> -t merged_upload [--upload-port <port>]
#
# The image ends with the updater, so flashing it leaves the filesystem after it
# alone. The env names its updater with custom_updater_env and the updater's slot
# with custom_updater_offset.

import os
import subprocess

Import("env")


def esptool(env):
    return os.path.join(env.PioPlatform().get_package_dir("tool-esptoolpy"), "esptool.py")


# Builds the merged image and returns its path
def merge(env):
    updater_env = env.GetProjectOption("custom_updater_env")
    updater_offset = env.GetProjectOption("custom_updater_offset")

    # A separate PlatformIO run, as the updater is an env of its own
    subprocess.check_call(
        [env.subst("$PYTHONEXE"), "-m", "platformio", "run", "-e", updater_env],
        cwd=env.subst("$PROJECT_DIR"),
    )
    updater = os.path.join(env.subst("$PROJECT_BUILD_DIR"), updater_env, "firmware.bin")

    images = [(offset, env.subst(path)) for offset, path in env.get("FLASH_EXTRA_IMAGES", [])]
    images.append((env.subst("$ESP32_APP_OFFSET"), env.subst("$BUILD_DIR/${PROGNAME}.bin")))
    images.append((updater_offset, updater))
    images.sort(key=lambda image: int(image[0], 16))

    output = env.subst("$BUILD_DIR/rrptr-${PIOENV}-merged.bin")
    command = [
        env.subst("$PYTHONEXE"),
        esptool(env),
        "--chip", env.BoardConfig().get("build.mcu"),
        "merge_bin", "-o", output,
        "--flash_mode", env.BoardConfig().get("build.flash_mode", "dio"),
        "--flash_size", env.BoardConfig().get("upload.flash_size", "4MB"),
    ]
    for offset, path in images:
        command += [offset, path]
    subprocess.check_call(command)
    print("Merged image: " + output)
    return output


def build_merged_image(target, source, env):
    output = merge(env)
    print("Flash it with: esptool.py --chip %s write_flash 0x0 %s" % (env.BoardConfig().get("build.mcu"), os.path.basename(output)))


def upload_merged_image(target, source, env):
    output = merge(env)
    env.AutodetectUploadPort()
    command = [env.subst("$PYTHONEXE"), esptool(env), "--chip", env.BoardConfig().get("build.mcu")]
    port = env.subst("$UPLOAD_PORT")
    if port:
        command += ["--port", port]
    command += ["--baud", env.subst("$UPLOAD_SPEED") or "921600", "write_flash", "0x0", output]
    if subprocess.call(command) != 0:
        print("Merged upload failed: see esptool's message above")
        env.Exit(1)


env.AddCustomTarget(
    name="merged",
    dependencies="$BUILD_DIR/${PROGNAME}.bin",
    actions=[build_merged_image],
    title="Merged image",
    description="Merge Rrptr and the OTA updater into one image, flashed at address 0",
)

env.AddCustomTarget(
    name="merged_upload",
    dependencies="$BUILD_DIR/${PROGNAME}.bin",
    actions=[upload_merged_image],
    title="Merged upload",
    description="Build the merged image and flash it over USB at address 0 (Rrptr and the OTA updater)",
)
