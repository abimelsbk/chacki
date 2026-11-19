# PlatformIO post-build script.
# Produces two images in ./dist after every successful build:
#   Chacki-CoreS3SE-launcher.bin  -> app-only image. Put it on the SD card and install it with M5 Launcher.
#   Chacki-CoreS3SE-full.bin      -> merged image (bootloader + partitions + app) for flashing at 0x0
#                                    with esptool / M5Burner / a web flasher. Flashing this REPLACES Launcher.
import os
import shutil
import subprocess

Import("env")  # noqa: F821  (provided by PlatformIO/SCons)


def make_bins(source, target, env):
    build_dir = env.subst("$BUILD_DIR")
    project_dir = env.subst("$PROJECT_DIR")
    dist = os.path.join(project_dir, "dist")
    os.makedirs(dist, exist_ok=True)

    app_bin = os.path.join(build_dir, "firmware.bin")
    launcher_bin = os.path.join(dist, "Chacki-CoreS3SE-launcher.bin")
    shutil.copyfile(app_bin, launcher_bin)
    print("[chacki] Launcher image : %s (%d KB)" % (launcher_bin, os.path.getsize(launcher_bin) // 1024))

    full_bin = os.path.join(dist, "Chacki-CoreS3SE-full.bin")
    esptool_dir = env.PioPlatform().get_package_dir("tool-esptoolpy")
    board = env.BoardConfig()
    cmd = [
        env.subst("$PYTHONEXE"), os.path.join(esptool_dir, "esptool.py"),
        "--chip", "esp32s3", "merge_bin", "-o", full_bin,
        "--flash_mode", board.get("build.flash_mode", "dio"),
        "--flash_size", "16MB",
    ]
    for offset, image in env.get("FLASH_EXTRA_IMAGES", []):
        cmd += [offset, env.subst(image)]
    cmd += [env.subst("$ESP32_APP_OFFSET") or "0x10000", app_bin]
    try:
        subprocess.check_call(cmd, stdout=subprocess.DEVNULL)
        print("[chacki] Full flash image: %s (flash at 0x0)" % full_bin)
    except Exception as exc:  # merged image is a convenience; never fail the build for it
        print("[chacki] Could not build merged image: %s" % exc)


env.AddPostAction("$BUILD_DIR/firmware.bin", make_bins)  # noqa: F821
