#!/usr/bin/env python3
"""Build the game for every device and put the files to flash in releases/.

Every file is named <device>_<game><variant>.<ext>, for example PicoSystem_Znax.uf2:

  ESPboy         .bin   the board is a LOLIN(WEMOS) D1 mini
  GamebuinoMeta  .bin   copy it into a folder on the SD card, the .hex is for flashing it directly
  PyBadge        .uf2   double press reset and copy it onto the drive that appears
  PyGamer        .uf2   same as the PyBadge
  PicoSystem     .uf2   hold X while switching on and copy it onto the drive that appears
  Explorer       .uf2   hold BOOT while pressing RESET and copy it onto the drive that appears
  Tufty          .uf2   hold HOME while pressing RESET and copy it onto the drive that appears
  ThumbyColor   .uf2   put it into bootloader mode and copy it onto the RPI-RP2 drive that appears
  Windows        .exe   linked statically, it runs on its own without a console window
  Web            .zip   index.html, .js and .wasm, ready to upload to an itch.io HTML project
  DOS            .zip   a 32 bit MS-DOS program with its DPMI host built in, for DOSBox or a real PC
  Playdate       .pdx.zip  unzip it and sideload the .pdx, it runs on the device and in the simulator
  Libretro       .zip   the core (<game>_libretro.dll) and its .info for RetroArch's cores and info folders
  GBA            .gba   a Game Boy Advance ROM, for an emulator or a flash cart
  NDS            .nds   a Nintendo DS card image, for an emulator or a flash card
  3DS            .3dsx  a Nintendo 3DS program, for an emulator or the Homebrew Launcher
  PSX            .exe   a PlayStation program, for an emulator or a console that runs one
  N64            .z64   a Nintendo 64 ROM, for an emulator or a flash cart
  PSP            .PBP   copy it as EBOOT.PBP into ms0:/PSP/GAME/<game>/ on the memory stick, or open it in PPSSPP
  Vita           .vpk   install it with VitaShell on a Vita with homebrew enabled, or open it in Vita3K

The settings of a build (SCREENBUFFER, SCALESCREEN, ...) are passed to the compiler as defines, the
device headers only use their own values for what a build does not set. The sources are not
touched. What is built for each device is listed in TARGETS below.

Needs the Arduino IDE 1.8 folder with the board packages (arduino-builder) and, for the Windows
build, MSYS2 with the mingw64 cmake, ninja, gcc and SDL2 packages. The Playdate build needs the Playdate
SDK and the ARM gcc of its instructions, and MSYS2's gcc for the simulator's dll. The libretro core needs
libretro-common (--libretro-common) and MSYS2's cmake, ninja and gcc. The GBA build needs devkitARM and
libgba (--devkitpro), the DS build devkitARM, libnds and calico from the same folder and the 3DS build devkitARM and libctru, and the PlayStation build PSn00bSDK (--psn00bsdk) and the Nintendo 64 build the mips64-elf
toolchain with libdragon (--n64).

Usage:
  python tools/build_releases.py                 build everything
  python tools/build_releases.py --only PicoSystem GamebuinoMeta
  python tools/build_releases.py --forcedebug    every build shows the debug header (FORCEDEBUG 1)
  python tools/build_releases.py --forceskin 1   every build has only skin 1
  python tools/build_releases.py --forcescreenbuffer 8   every build draws into an 8 bpp buffer
  python tools/build_releases.py --list          show what would be built

  --arduino DIR   the Arduino IDE folder (default C:/arduino, or the ARDUINO_DIR environment variable)
  --msys2 DIR     MSYS2's mingw64 bin folder (default C:/msys64/mingw64/bin, or MSYS2_BIN). Where
                  that folder is not there, cmake and ninja are taken from the PATH
  --arduino-cli PATH  build the Arduino devices with arduino-cli instead of the Arduino IDE 1.8
                  folder (default ARDUINO_CLI). This is what the CI workflow uses
  --lovyangfx DIR the LovyanGFX folder the Windows build uses, when it is not the one in the Arduino
                  IDE's sketchbook (default LOVYANGFX_DIR)
  --cross-windows the Windows exe, the libretro dll and the Playdate simulator's pdex.dll are built
                  on Linux with mingw-w64, see tools/mingw-w64.cmake
  --sdl2-mingw DIR  SDL2's mingw package, its x86_64-w64-mingw32 folder (default SDL2_MINGW)
  --playdate-sdk DIR  the Playdate SDK folder (default PLAYDATE_SDK_PATH, or C:/playdate/PlaydateSDK)
  --playdate-arm DIR  the bin folder of the ARM gcc for the Playdate (default PLAYDATE_ARM_BIN, or
                      C:/playdate/arm-gnu-toolchain-14.2.rel1-mingw-w64-i686-arm-none-eabi/bin)
  --libretro-common DIR  the libretro-common folder (default LIBRETRO_COMMON_DIR, or C:/github/libretro-common)
  --devkitpro DIR the folder with devkitARM, libgba, libnds, calico and tools (default DEVKITPRO, or C:/devkitarm)
  --pspdev DIR    the pspdev toolchain for the PSP (default PSPDEV_DIR, or C:/psp_dev)
  --vitasdk DIR   VitaSDK for the PlayStation Vita (default VITASDK, or C:/psvita_dev)
  --psn00bsdk DIR the folder PSn00bSDK is installed in (default PSN00BSDK_PREFIX, or C:/psn00bsdk)
  --n64 DIR       the folder with the mips64-elf toolchain and libdragon (default N64_INST, or
                  C:/n64_dev)
  --emsdk DIR     the Emscripten SDK for the browser build (default EMSDK, or C:/github/emsdk)
  --dosdev DIR    DJGPP and CWSDPMI for the MS-DOS build (default DOSDEV, or C:/dos_dev)
"""
import argparse
import glob
import hashlib
import json
import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import time

# ============================================================================================
# The game
# ============================================================================================

GAME = "Znax"
SKETCH = "znax_embedded"
CMAKE_TARGET = "znax"

# how many skins the game has, so --forceskin takes -1 or 0 to SKINS - 1. See FORCESKIN in defines.h
SKINS = 2

# (device, variant added to the file name, defines)
TARGETS = [
    ("ESPboy", "", {}),
    ("GamebuinoMeta", "", {}),
    ("PyBadge", "", {}),
    ("PyGamer", "", {}),
    ("PicoSystem", "", {}),
    ("Explorer", "", {}),
    ("Tufty", "", {}),
    ("ThumbyColor", "", {}),
    ("Windows", "", {}),
    ("Web", "", {}),
    ("DOS", "", {}),
    ("Playdate", "", {}),
    ("Libretro", "", {}),
    ("GBA", "", {}),
    ("NDS", "", {}),
    ("3DS", "", {}),
    ("PSX", "", {}),
    ("N64", "", {}),
    ("PSP", "", {}),
    ("Vita", "", {}),
]

# ============================================================================================
# The devices
# ============================================================================================

# fqbn: board and its menu options. outputs: the build's file for each extension that is released,
# "uf2 from bin" makes the UF2 here out of the .bin (base address and UF2 family of the bootloader)
DEVICES = {
    "ESPboy": {
        "fqbn": "esp8266:esp8266:d1_mini:xtal=160,vt=flash,exception=disabled,stacksmash=disabled,ssl=basic,"
                "mmu=3232,non32xfer=fast,eesz=4M2M,ip=lm2f,dbg=Disabled,lvl=None____,wipe=none,baud=921600",
        "outputs": ["bin"],
    },
    "GamebuinoMeta": {
        "fqbn": "gamebuino:samd:gamebuino_meta_native",
        # gcc 7 is what this core's toolsDependencies name, see toolchain_pref. gcc 9 compiles
        # "x % 80" to a multiply and a RORS where gcc 7 calls __aeabi_uidivmod, and RORS is the one
        # ARMv6-M instruction the emulator on gamebuino.com cannot decode: a game built with gcc 9
        # dies there with "NO INSTRUCTIONHANDLER". The META itself runs either one
        "toolchain": ("arm-none-eabi-gcc", "7-2017q4"),
        # the core builds at -Os; this game has the flash to spare for -O3
        "optimize": "-O3",
        # "zip from bin" makes the folder the META's loader wants, see package_with_data
        "outputs": ["zip from bin"],
        "data": "gamebuino",
        # the .hex goes in the folder as well, it is what flashes the game over USB
        "also": [".hex"],
        # The folder it takes on the card, which is also where the game writes its save. Other
        # games are called Sokoban and Waternet already, so these carry this repository's name
        # and sit beside them instead of on top of them
        "folder": GAME + "_embedded",
    },
    "PyBadge": {
        "fqbn": "adafruit:samd:adafruit_pybadge_m4",
        "toolchain": ("arm-none-eabi-gcc", "9-2019q4"),
        "outputs": ["uf2 from bin"],
        "uf2": (0x4000, 0x55114460),
    },
    "PyGamer": {
        "fqbn": "adafruit:samd:adafruit_pygamer_m4",
        "toolchain": ("arm-none-eabi-gcc", "9-2019q4"),
        "outputs": ["uf2 from bin"],
        "uf2": (0x4000, 0x55114460),
    },
    "PicoSystem": {
        "fqbn": "rp2040:rp2040:generic:flash=16777216_0,boot2=boot2_w25q080_2_padded_checksum,freq=133,usbstack=picosdk,opt=Small",
        "outputs": ["uf2"],
    },
    "Explorer": {
        "fqbn": "rp2040:rp2040:pimoroni_explorer:flash=16777216_0,arch=arm,freq=150,usbstack=picosdk,opt=Small",
        "outputs": ["uf2"],
    },
    "Tufty": {
        "fqbn": "rp2040:rp2040:generic_rp2350:variantchip=RP2530B,psramcs=GPIO8,psram=8mb,flash=16777216_0,arch=arm,freq=150,usbstack=picosdk,opt=Small",
        "outputs": ["uf2"],
    },
    # arduino-pico has no Thumby Color board, it builds as the Generic RP2350 with the RP2350A chip
    # variant, which is also what tells it apart from the Tufty in PlatformDevice.h
    "ThumbyColor": {
        "fqbn": "rp2040:rp2040:generic_rp2350:variantchip=RP2350A,flash=16777216_0,arch=arm,freq=150,usbstack=picosdk,opt=Small",
        "outputs": ["uf2"],
    },
    "Windows": {
        "cmake": True,
        "outputs": ["exe"],
    },
    # built from playdate/, see build_playdate
    "Playdate": {
        "playdate": True,
        "outputs": ["pdx.zip"],
    },
    # built from libretro/, see build_libretro
    "Libretro": {
        "libretro": True,
        "outputs": ["zip"],
    },
    # built from gba/, see build_gba
    "GBA": {
        "gba": True,
        "outputs": ["gba"],
    },
    # built from nds/, see build_nds
    "NDS": {
        "nds": True,
        "outputs": ["nds"],
    },
    # built from 3ds/, see build_3ds
    "3DS": {
        "3ds": True,
        "outputs": ["3dsx"],
    },
    # built from psx/, see build_psx
    "PSX": {
        "psx": True,
        "outputs": ["exe"],
    },
    # built from n64/, see build_n64
    "N64": {
        "n64": True,
        "outputs": ["z64"],
    },
    # built from web/, see build_web
    "Web": {
        "web": True,
        "outputs": ["zip"],
    },
    # built from dos/, see build_dos
    "DOS": {
        "dos": True,
        "outputs": ["zip"],
    },
    # built from vita/, see build_vita
    "Vita": {
        "vita": True,
        "outputs": ["vpk"],
    },
    # built from psp/, see build_psp
    "PSP": {
        "psp": True,
        "outputs": ["PBP"],
    },
}

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, ".."))
SOURCE = os.path.join(ROOT, "source", SKETCH)
RELEASES = os.path.join(ROOT, "releases")
# every platform keeps its build files in a folder of its own under here
PLATFORMS = os.path.join(ROOT, "platforms")
# what a device wants on its card beside the game itself, a folder for each of them
PLATFORM_DATA = os.path.join(ROOT, "platform_data")
WORK = os.path.join(tempfile.gettempdir(), SKETCH + "_releases")


def file_name(device, variant, ext):
    return "%s_%s%s.%s" % (device, GAME, variant, ext)


def bin_to_uf2(data, base, family):
    """The .bin as UF2 blocks of 256 bytes each, for a bootloader that takes the given family"""
    count = (len(data) + 255) // 256
    out = bytearray()
    for i in range(count):
        chunk = data[i * 256:(i + 1) * 256]
        out += struct.pack("<8I", 0x0A324655, 0x9E5D5157, 0x00002000, base + i * 256, 256, i, count, family)
        out += chunk + bytes(476 - len(chunk))
        out += struct.pack("<I", 0x0AB16F30)
    return bytes(out)


def package_with_data(binary, folder_name, data_name, target, also=()):
    """Zips the game up the way a device's card wants it: a folder named after the game holding the
    binary, and beside it whatever platform_data has for that device (the META's loader reads its
    ICON.BMP and TITLESCREEN.BMP from there). Unpacking the zip on the card is then the whole job"""
    stage = os.path.join(WORK, "package_" + data_name)
    shutil.rmtree(stage, ignore_errors=True)
    folder = os.path.join(stage, folder_name)
    os.makedirs(folder)
    shutil.copyfile(binary, os.path.join(folder, folder_name + ".bin"))
    # whatever else the build made that belongs with it, under the same name too
    for extra in also:
        shutil.copyfile(extra, os.path.join(folder, folder_name + os.path.splitext(extra)[1]))
    data = os.path.join(PLATFORM_DATA, data_name)
    if os.path.isdir(data):
        for name in sorted(os.listdir(data)):
            source = os.path.join(data, name)
            if os.path.isfile(source):
                shutil.copyfile(source, os.path.join(folder, name))
    # the archive is made outside the folder it packs, so it can not end up inside itself
    archive = shutil.make_archive(os.path.join(WORK, data_name + "_package"), "zip",
                                  root_dir=stage, base_dir=folder_name)
    shutil.copyfile(archive, target)
    shutil.rmtree(stage, ignore_errors=True)


def define_flags(defines):
    return " ".join("-D%s=%s" % (name, value) for name, value in sorted(defines.items()))


def cache_tag(device, defines):
    """Part of the core cache's folder name, so that every set of compiler settings gets a cache of
    its own. arduino-builder keys its cache on the board alone, so a core compiled at -Os, or with
    another toolchain, is quietly reused once those change and the build ends up with objects from
    both. Anything that changes how the core is compiled belongs in here"""
    settings = "%s|%s" % (extra_flags(device, defines), DEVICES[device].get("toolchain", ""))
    return hashlib.sha1(settings.encode()).hexdigest()[:8]


def extra_flags(device, defines):
    """What goes into compiler.c/cpp.extra_flags: the build's defines, and the optimisation level
    where a device asks for one of its own. The recipe puts extra_flags after the core's own flags,
    so a -O here is the one that counts"""
    flags = define_flags(defines)
    optimize = DEVICES[device].get("optimize")
    return (flags + " " + optimize).strip() if optimize else flags


def tool_env(extra_dirs):
    """The environment with the given folders in front of PATH, skipping the ones that do not exist.
    On a machine without MSYS2 (a Linux runner) the tools come from the PATH as they are"""
    env = dict(os.environ)
    dirs = [d for d in extra_dirs if d and os.path.isdir(d)]
    if dirs:
        env["PATH"] = os.pathsep.join(dirs) + os.pathsep + env.get("PATH", "")
    return env


def tool(directory, name):
    """The tool in directory, or its bare name when that folder is not there and the PATH has it"""
    exe = name + (".exe" if os.name == "nt" else "")
    if directory and os.path.isfile(os.path.join(directory, exe)):
        return os.path.join(directory, exe)
    return name


def cross_settings(cross):
    """The CMake settings that make a build produce Windows binaries from Linux with mingw-w64, or
    nothing at all when the build is a native one. cross is the SDL2 mingw folder, or True when the
    build needs no SDL2"""
    if not cross:
        return []
    settings = ["-DCMAKE_TOOLCHAIN_FILE=" + os.path.join(HERE, "mingw-w64.cmake").replace(os.sep, "/")]
    if isinstance(cross, str):
        # where find_package(SDL2) looks: the mingw package's x86_64-w64-mingw32 folder
        settings.append("-DCMAKE_PREFIX_PATH=" + cross.replace(os.sep, "/"))
    return settings


def windows_binaries(cross):
    """True when the build is making Windows binaries, natively or by cross compiling"""
    return (os.name == "nt") or bool(cross)


def toolchain_pref(device, packages, log):
    """The runtime.tools pref that pins a device's compiler, or "" when it pins none.

    A core names the compiler it wants in its toolsDependencies, but its platform.txt asks for it
    as {runtime.tools.arm-none-eabi-gcc.path}, without the version. With several cores installed
    side by side that leaves the builder free to take any arm-none-eabi-gcc it finds, and it takes
    the newest rather than the one the core asks for. The Arduino IDE folder and the CI's
    arduino-cli both hold the Gamebuino, Adafruit and Arduino SAMD cores at once, so both are
    affected. Returns None, and writes why into the log, when the pinned version is not installed"""
    pin = DEVICES[device].get("toolchain")
    if not pin:
        return ""
    name, version = pin
    found = sorted(glob.glob(os.path.join(packages, "*", "tools", name, version)))
    if not found:
        with open(log, "w") as f:
            f.write("%s pins %s %s, which is not installed under %s.\n"
                    % (device, name, version, packages))
            f.write("Install the core that brings it, or drop the pin from DEVICES.\n")
        return None
    return "runtime.tools.%s.path=%s" % (name, found[0].replace(os.sep, "/"))


def config_data_dir(node):
    """directories.data out of an arduino-cli config dump, wherever the version nests it"""
    if isinstance(node, dict):
        directories = node.get("directories")
        if isinstance(directories, dict) and isinstance(directories.get("data"), str):
            return directories["data"]
        for value in node.values():
            found = config_data_dir(value)
            if found:
                return found
    return ""


def arduino_cli_packages(arduino_cli):
    """Where arduino-cli keeps the installed cores and their tools. "config dump" is asked rather
    than "config get", which older arduino-cli versions answer with their help text"""
    data = ""
    try:
        result = subprocess.run([arduino_cli, "config", "dump", "--format", "json"],
                                capture_output=True, text=True)
        data = config_data_dir(json.loads(result.stdout))
    except (OSError, ValueError):
        data = ""
    if not os.path.isdir(data):
        # what arduino-cli falls back to itself, and where the CI runner keeps it
        data = os.path.join(os.path.expanduser("~"), ".arduino15")
    return os.path.join(data, "packages")


def build_arduino_cli(device, defines, build_dir, cache_dir, arduino_cli, log):
    """Builds the sketch with arduino-cli, which is what the build uses where there is no Arduino IDE
    1.8 folder (the CI runners). Returns the path of the build's files without extension"""
    flags = extra_flags(device, defines)
    toolchain = toolchain_pref(device, arduino_cli_packages(arduino_cli), log)
    if toolchain is None:
        return None
    command = [
        arduino_cli, "compile",
        "--fqbn", DEVICES[device]["fqbn"],
        "--build-path", build_dir,
        "--build-cache-path", cache_dir,
        # empty in every one of the board packages, so the defines are all they hold
        "--build-property", "compiler.c.extra_flags=" + flags,
        "--build-property", "compiler.cpp.extra_flags=" + flags,
    ]
    # the compiler the device's core asks for, where it does not leave the choice open
    if toolchain:
        command += ["--build-property", toolchain]
    command.append(SOURCE)
    with open(log, "w") as f:
        result = subprocess.run(command, stdout=f, stderr=subprocess.STDOUT)
    if result.returncode != 0:
        return None
    return os.path.join(build_dir, SKETCH + ".ino")


def build_arduino(device, defines, build_dir, cache_dir, arduino, log):
    """Builds the sketch with arduino-builder, returns the path of the build's files without extension"""
    portable = os.path.join(arduino, "portable")
    flags = extra_flags(device, defines)
    toolchain = toolchain_pref(device, os.path.join(portable, "packages"), log)
    if toolchain is None:
        return None
    command = [
        os.path.join(arduino, "arduino-builder.exe" if os.name == "nt" else "arduino-builder"),
        "-compile", "-logger=human",
        "-hardware", os.path.join(arduino, "hardware"),
        "-hardware", os.path.join(portable, "packages"),
        "-tools", os.path.join(arduino, "tools-builder"),
        "-tools", os.path.join(arduino, "hardware", "tools", "avr"),
        "-tools", os.path.join(portable, "packages"),
        "-built-in-libraries", os.path.join(arduino, "libraries"),
        "-libraries", os.path.join(portable, "sketchbook", "libraries"),
        "-fqbn", DEVICES[device]["fqbn"],
        "-ide-version=10819",
        "-build-path", build_dir,
        "-build-cache", cache_dir,
        # empty in every one of the board packages, so the defines are all they hold
        "-prefs", "compiler.c.extra_flags=" + flags,
        "-prefs", "compiler.cpp.extra_flags=" + flags,
    ]
    # the compiler the device's core asks for, where it does not leave the choice open
    if toolchain:
        command += ["-prefs", toolchain]
    command.append(os.path.join(SOURCE, SKETCH + ".ino"))
    with open(log, "w") as f:
        result = subprocess.run(command, stdout=f, stderr=subprocess.STDOUT)
    if result.returncode != 0:
        return None
    return os.path.join(build_dir, SKETCH + ".ino")


def build_windows(defines, build_dir, msys2, cross, lovyangfx, log):
    """Builds the exe with CMake and ninja from MSYS2, or with mingw-w64 when cross compiling from
    Linux, and returns the path of the exe without extension"""
    env = tool_env([msys2])
    cmake = tool(msys2, "cmake")
    configure = [cmake, "-S", os.path.join(PLATFORMS, "windows"), "-B", build_dir, "-G", "Ninja",
             "-DCMAKE_BUILD_TYPE=Release"]
    if lovyangfx:
        # where LovyanGFX is, when it is not in the Arduino IDE's sketchbook the CMakeLists expects
        configure.append("-DLOVYANGFX_DIR=" + lovyangfx.replace(os.sep, "/"))
    configure += cross_settings(cross)
    configure += ["-D%s=%s" % (name, value) for name, value in sorted(defines.items())]
    with open(log, "w") as f:
        for command in (configure, [cmake, "--build", build_dir]):
            if subprocess.run(command, stdout=f, stderr=subprocess.STDOUT, env=env).returncode != 0:
                return None
    return os.path.join(build_dir, CMAKE_TARGET)


def build_playdate(defines, build_dir, msys2, sdk, arm, cross, log):
    """Builds the pdx for the Playdate device and its simulator with CMake and ninja, returns the path of
    the zipped pdx without extension. The playdate folder is copied into build_dir first: each build
    puts its pdex into Source/ there, so the pdx made last has both and the sources stay untouched"""
    project = os.path.join(build_dir, "playdate")
    shutil.copytree(os.path.join(PLATFORMS, "playdate"), project, ignore=shutil.ignore_patterns("pdex.*", "*.pdx"))
    env = dict(os.environ)
    env["PLAYDATE_SDK_PATH"] = sdk
    env.update(tool_env([arm, msys2]))
    cmake = tool(msys2, "cmake")
    common = ["-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release", "-DGAME_DIR=" + SOURCE, "-DPRODUCT_DIR=" + build_dir]
    common += ["-D%s=%s" % (name, value) for name, value in sorted(defines.items())]
    device = os.path.join(build_dir, "device")
    simulator = os.path.join(build_dir, "simulator")
    toolchain = os.path.join(sdk, "C_API", "buildsupport", "arm.cmake").replace(os.sep, "/")
    commands = [
        [cmake, "-S", project, "-B", device, "-DCMAKE_TOOLCHAIN_FILE=" + toolchain] + common,
        [cmake, "--build", device],
        [cmake, "-S", project, "-B", simulator] + cross_settings(cross) + common,
        [cmake, "--build", simulator],
    ]
    with open(log, "w") as f:
        for command in commands:
            if subprocess.run(command, stdout=f, stderr=subprocess.STDOUT, env=env).returncode != 0:
                return None
    pdx = GAME + ".pdx"
    if not os.path.isdir(os.path.join(build_dir, pdx)):
        return None
    shutil.make_archive(os.path.join(build_dir, pdx), "zip", root_dir=build_dir, base_dir=pdx)
    return os.path.join(build_dir, GAME)


def build_libretro(defines, build_dir, msys2, libretro_common, cross, log):
    """Builds the libretro core with CMake and ninja from MSYS2 and zips it with its .info file,
    returns the path of the zip without extension"""
    env = tool_env([msys2])
    cmake = tool(msys2, "cmake")
    configure = [cmake, "-S", os.path.join(PLATFORMS, "libretro"), "-B", build_dir, "-G", "Ninja",
                 "-DCMAKE_BUILD_TYPE=Release", "-DLIBRETRO_COMMON_DIR=" + libretro_common]
    configure += cross_settings(cross)
    configure += ["-D%s=%s" % (name, value) for name, value in sorted(defines.items())]
    with open(log, "w") as f:
        for command in (configure, [cmake, "--build", build_dir]):
            if subprocess.run(command, stdout=f, stderr=subprocess.STDOUT, env=env).returncode != 0:
                return None
    core = CMAKE_TARGET + "_libretro"
    extension = ".dll" if windows_binaries(cross) else ".so"
    files = [core + extension, core + ".info"]
    if not all(os.path.isfile(os.path.join(build_dir, name)) for name in files):
        return None
    package = os.path.join(build_dir, "package")
    os.makedirs(package)
    for name in files:
        shutil.copyfile(os.path.join(build_dir, name), os.path.join(package, name))
    shutil.make_archive(os.path.join(build_dir, "core"), "zip", root_dir=package)
    return os.path.join(build_dir, "core")


def build_gba(defines, build_dir, msys2, devkitpro, log):
    """Builds the Game Boy Advance ROM with CMake and ninja from MSYS2, returns the path of the ROM
    without extension. The screen buffer goes in the GBA's fast memory, and when the game's globals
    leave no room for it there the build is done again with it in the work RAM"""
    env = tool_env([msys2])
    cmake = tool(msys2, "cmake")
    common = ["-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release", "-DDEVKITPRO=" + devkitpro]
    common += ["-D%s=%s" % (name, value) for name, value in sorted(defines.items())]
    for buffer_in_iwram in (1, 0):
        if "BUFFERINIWRAM" in defines and buffer_in_iwram:
            continue
        shutil.rmtree(build_dir, ignore_errors=True)
        os.makedirs(build_dir)
        settings = list(common)
        if "BUFFERINIWRAM" not in defines:
            settings.append("-DBUFFERINIWRAM=%d" % buffer_in_iwram)
        with open(log, "w") as f:
            failed = False
            for command in ([cmake, "-S", os.path.join(PLATFORMS, "gba"), "-B", build_dir] + settings, [cmake, "--build", build_dir]):
                if subprocess.run(command, stdout=f, stderr=subprocess.STDOUT, env=env).returncode != 0:
                    failed = True
                    break
        if not failed:
            return os.path.join(build_dir, GAME)
        with open(log, errors="replace") as f:
            log_text = f.read()
        overflowed = ("iwram' overflowed" in log_text) or ("within region `iwram'" in log_text)
        if not overflowed:
            return None
        # no room in the fast memory for the buffer, the work RAM it is
    return None


def build_nds(defines, build_dir, msys2, devkitpro, log):
    """Builds the Nintendo DS card image with CMake and ninja from MSYS2, returns the path of the
    image without extension"""
    env = tool_env([msys2])
    cmake = tool(msys2, "cmake")
    settings = ["-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release", "-DDEVKITPRO=" + devkitpro]
    settings += ["-D%s=%s" % (name, value) for name, value in sorted(defines.items())]
    shutil.rmtree(build_dir, ignore_errors=True)
    os.makedirs(build_dir)
    with open(log, "w") as f:
        for command in ([cmake, "-S", os.path.join(PLATFORMS, "nds"), "-B", build_dir] + settings, [cmake, "--build", build_dir]):
            if subprocess.run(command, stdout=f, stderr=subprocess.STDOUT, env=env).returncode != 0:
                return None
    return os.path.join(build_dir, GAME)


def build_3ds(defines, build_dir, msys2, devkitpro, log):
    """Builds the Nintendo 3DS program with CMake and ninja from MSYS2, returns the path of the
    program without extension"""
    env = tool_env([msys2])
    cmake = tool(msys2, "cmake")
    settings = ["-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release", "-DDEVKITPRO=" + devkitpro]
    settings += ["-D%s=%s" % (name, value) for name, value in sorted(defines.items())]
    shutil.rmtree(build_dir, ignore_errors=True)
    os.makedirs(build_dir)
    with open(log, "w") as f:
        for command in ([cmake, "-S", os.path.join(PLATFORMS, "3ds"), "-B", build_dir] + settings, [cmake, "--build", build_dir]):
            if subprocess.run(command, stdout=f, stderr=subprocess.STDOUT, env=env).returncode != 0:
                return None
    return os.path.join(build_dir, GAME)


def build_psx(defines, build_dir, msys2, psn00bsdk, log):
    """Builds the PlayStation's PS-EXE with CMake and ninja from MSYS2, returns the path of the
    program without extension"""
    env = tool_env([msys2])
    cmake = tool(msys2, "cmake")
    settings = ["-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release", "-DPSN00BSDK_PREFIX=" + psn00bsdk]
    settings += ["-D%s=%s" % (name, value) for name, value in sorted(defines.items())]
    shutil.rmtree(build_dir, ignore_errors=True)
    os.makedirs(build_dir)
    with open(log, "w") as f:
        for command in ([cmake, "-S", os.path.join(PLATFORMS, "psx"), "-B", build_dir] + settings, [cmake, "--build", build_dir]):
            if subprocess.run(command, stdout=f, stderr=subprocess.STDOUT, env=env).returncode != 0:
                return None
    return os.path.join(build_dir, GAME)


def build_dos(defines, build_dir, msys2, dosdev, log):
    """Builds the MS-DOS program with DJGPP and zips it, returns the path of the zip without
    extension. It is zipped rather than released on its own because DOS only takes eight characters
    and three, and the name the release files carry is longer than that"""
    env = tool_env([msys2])
    cmake = tool(msys2, "cmake")
    settings = ["-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release", "-DDOSDEV=" + dosdev]
    settings += ["-D%s=%s" % (name, value) for name, value in sorted(defines.items())]
    shutil.rmtree(build_dir, ignore_errors=True)
    os.makedirs(build_dir)
    with open(log, "w") as f:
        for command in ([cmake, "-S", os.path.join(PLATFORMS, "dos"), "-B", build_dir] + settings, [cmake, "--build", build_dir]):
            if subprocess.run(command, stdout=f, stderr=subprocess.STDOUT, env=env).returncode != 0:
                return None
    program = GAME.upper()[:8] + ".EXE"
    if not os.path.isfile(os.path.join(build_dir, program)):
        return None
    package = os.path.join(build_dir, "package")
    os.makedirs(package)
    shutil.copyfile(os.path.join(build_dir, program), os.path.join(package, program))
    shutil.make_archive(os.path.join(build_dir, "dos"), "zip", root_dir=package)
    return os.path.join(build_dir, "dos")


def build_web(defines, build_dir, msys2, emsdk, log):
    """Builds the browser version with Emscripten and zips the three files an itch.io HTML project
    takes, returns the path of the zip without extension"""
    env = tool_env([msys2])
    cmake = tool(msys2, "cmake")
    settings = ["-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release", "-DEMSDK=" + emsdk]
    settings += ["-D%s=%s" % (name, value) for name, value in sorted(defines.items())]
    shutil.rmtree(build_dir, ignore_errors=True)
    os.makedirs(build_dir)
    with open(log, "w") as f:
        for command in ([cmake, "-S", os.path.join(PLATFORMS, "web"), "-B", build_dir] + settings, [cmake, "--build", build_dir]):
            if subprocess.run(command, stdout=f, stderr=subprocess.STDOUT, env=env).returncode != 0:
                return None
    # itch.io opens the index.html at the root of the zip, the other two sit next to it
    files = ["index.html", "index.js", "index.wasm"]
    if not all(os.path.isfile(os.path.join(build_dir, name)) for name in files):
        return None
    package = os.path.join(build_dir, "package")
    os.makedirs(package)
    for name in files:
        shutil.copyfile(os.path.join(build_dir, name), os.path.join(package, name))
    shutil.make_archive(os.path.join(build_dir, "web"), "zip", root_dir=package)
    return os.path.join(build_dir, "web")


def build_n64(defines, build_dir, msys2, n64, log):
    """Builds the Nintendo 64 ROM with CMake and ninja from MSYS2, returns the path of the ROM
    without extension"""
    env = tool_env([msys2])
    cmake = tool(msys2, "cmake")
    settings = ["-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release", "-DN64_INST=" + n64]
    settings += ["-D%s=%s" % (name, value) for name, value in sorted(defines.items())]
    shutil.rmtree(build_dir, ignore_errors=True)
    os.makedirs(build_dir)
    with open(log, "w") as f:
        for command in ([cmake, "-S", os.path.join(PLATFORMS, "n64"), "-B", build_dir] + settings, [cmake, "--build", build_dir]):
            if subprocess.run(command, stdout=f, stderr=subprocess.STDOUT, env=env).returncode != 0:
                return None
    return os.path.join(build_dir, GAME)


def build_psp(defines, build_dir, pspdev, log):
    """Builds the PSP's EBOOT.PBP, returns the path of the build's files without extension. pspdev
    has no Windows toolchain, so on Windows the build runs in WSL and writes into the same folder"""
    settings = ["-D%s=%s" % (name, value) for name, value in sorted(defines.items())]
    source = os.path.join(PLATFORMS, "psp")
    if os.name == "nt":
        # WSL reaches the Windows drives under /mnt, and PSPDEV is where the toolchain was unpacked
        def wsl_path(path):
            path = os.path.abspath(path).replace("\\", "/")
            return "/mnt/" + path[0].lower() + path[2:]
        # The build runs from a script and not from "wsl bash -c <line>": what is quoted on that
        # command line is taken apart again on the way into WSL, which mangles $ and brackets
        os.makedirs(WORK, exist_ok=True)
        script_path = os.path.join(WORK, "build_psp.sh")
        with open(script_path, "w", newline="\n") as f:
            f.write("#!/bin/bash\nset -e\n")
            f.write("export PSPDEV=%s\n" % wsl_path(pspdev))
            f.write('export PATH="$PSPDEV/bin:$PATH"\n')
            f.write("psp-cmake -S %s -B %s %s\n" % (wsl_path(source), wsl_path(build_dir), " ".join(settings)))
            f.write("cmake --build %s --parallel\n" % wsl_path(build_dir))
        command = ["wsl", "--", "bash", wsl_path(script_path)]
        env = dict(os.environ)
    else:
        env = dict(os.environ)
        env["PSPDEV"] = pspdev
        env["PATH"] = os.path.join(pspdev, "bin") + os.pathsep + env.get("PATH", "")
        command = None

    shutil.rmtree(build_dir, ignore_errors=True)
    os.makedirs(build_dir)
    with open(log, "w") as f:
        if command is None:
            commands = [[os.path.join(pspdev, "bin", "psp-cmake"), "-S", source, "-B", build_dir] + settings,
                        ["cmake", "--build", build_dir]]
        else:
            commands = [command]
        for one in commands:
            if subprocess.run(one, stdout=f, stderr=subprocess.STDOUT, env=env).returncode != 0:
                return None
    # the PBP is what the PSP starts, it is always called EBOOT.PBP
    eboot = os.path.join(build_dir, "EBOOT.PBP")
    if not os.path.exists(eboot):
        return None
    out = os.path.join(build_dir, GAME)
    shutil.copyfile(eboot, out + ".PBP")
    return out


def build_vita(defines, build_dir, msys2, vitasdk, log):
    """Builds the PlayStation Vita's VPK with CMake and ninja from MSYS2, returns the path of the
    build's files without extension"""
    env = tool_env([os.path.join(vitasdk, "bin"), msys2])
    env["VITASDK"] = vitasdk.replace("\\", "/")
    # VitaSDK's cmake files ask for a policy version today's CMake no longer keeps, and this is what
    # lets them configure anyway. It has to be in the environment so the try compiles see it too
    env["CMAKE_POLICY_VERSION_MINIMUM"] = "3.5"
    cmake = tool(msys2, "cmake")
    settings = ["-G", "Ninja",
                "-DCMAKE_TOOLCHAIN_FILE=%s/share/vita.toolchain.cmake" % vitasdk.replace("\\", "/")]
    settings += ["-D%s=%s" % (name, value) for name, value in sorted(defines.items())]

    shutil.rmtree(build_dir, ignore_errors=True)
    os.makedirs(build_dir)
    with open(log, "w") as f:
        for command in ([cmake, "-S", os.path.join(PLATFORMS, "vita"), "-B", build_dir] + settings,
                        [cmake, "--build", build_dir]):
            if subprocess.run(command, stdout=f, stderr=subprocess.STDOUT, env=env).returncode != 0:
                return None
    return os.path.join(build_dir, GAME)


def errors_in(log):
    with open(log, errors="replace") as f:
        lines = [l.rstrip() for l in f if re.search(r"error|overflowed|will not fit", l, re.I)]
    return lines[:8]


def main():
    parser = argparse.ArgumentParser(description="Build every device's release files into releases/")
    parser.add_argument("--only", nargs="+", metavar="DEVICE", help="only these devices: " + ", ".join(DEVICES))
    parser.add_argument("--forcedebug", action="store_true", help="show the debug header in every build")
    parser.add_argument("--forceskin", type=int, metavar="N", choices=range(-1, SKINS),
                        help="build every device with skin N: -1 is what the game does on its own, "
                             "0 to %d only that skin (see FORCESKIN in defines.h)" % (SKINS - 1))
    parser.add_argument("--forcescreenbuffer", type=int, metavar="N", choices=(0, 1, 8, 16),
                        help="build every device with SCREENBUFFER N: 0, 1, 8 or 16. Not every device "
                             "takes every mode, its CMakeLists.txt says which")
    parser.add_argument("--list", action="store_true", help="list the builds and exit")
    parser.add_argument("--arduino", default=os.environ.get("ARDUINO_DIR", "C:/arduino"))
    parser.add_argument("--arduino-cli", default=os.environ.get("ARDUINO_CLI", ""),
                        help="build the Arduino devices with this arduino-cli instead of the IDE folder")
    parser.add_argument("--lovyangfx", default=os.environ.get("LOVYANGFX_DIR", ""),
                        help="the LovyanGFX library folder for the Windows build, when it is not in "
                             "the Arduino IDE's sketchbook (default LOVYANGFX_DIR)")
    parser.add_argument("--cross-windows", action="store_true",
                        help="make the Windows exe, the libretro dll and the Playdate simulator's "
                             "pdex.dll on Linux with mingw-w64 (tools/mingw-w64.cmake)")
    parser.add_argument("--sdl2-mingw", default=os.environ.get("SDL2_MINGW", ""),
                        help="the x86_64-w64-mingw32 folder of SDL2's mingw package, for --cross-windows")
    parser.add_argument("--msys2", default=os.environ.get("MSYS2_BIN", "C:/msys64/mingw64/bin"))
    parser.add_argument("--playdate-sdk", default=os.environ.get("PLAYDATE_SDK_PATH", "C:/playdate/PlaydateSDK"))
    parser.add_argument("--devkitpro", default=os.environ.get("DEVKITPRO", "C:/devkitarm"))
    parser.add_argument("--psn00bsdk", default=os.environ.get("PSN00BSDK_PREFIX", "C:/psn00bsdk"))
    parser.add_argument("--n64", default=os.environ.get("N64_INST", "C:/n64_dev"))
    parser.add_argument("--emsdk", default=os.environ.get("EMSDK", "C:/github/emsdk"))
    parser.add_argument("--dosdev", default=os.environ.get("DOSDEV", "C:/dos_dev"))
    parser.add_argument("--pspdev", default=os.environ.get("PSPDEV_DIR", "C:/psp_dev"))
    parser.add_argument("--vitasdk", default=os.environ.get("VITASDK", "C:/psvita_dev"))
    parser.add_argument("--libretro-common", default=os.environ.get("LIBRETRO_COMMON_DIR", "C:/github/libretro-common"))
    parser.add_argument("--playdate-arm", default=os.environ.get("PLAYDATE_ARM_BIN", "C:/playdate/arm-gnu-toolchain-14.2.rel1-mingw-w64-i686-arm-none-eabi/bin"))
    args = parser.parse_args()

    # what the Windows producing builds are handed: the SDL2 folder when there is one, otherwise just
    # "cross compile". None means a native build
    cross = None
    if args.cross_windows:
        cross = args.sdl2_mingw or True

    only = None
    if args.only:
        names = {d.lower(): d for d in DEVICES}
        unknown = [d for d in args.only if d.lower() not in names]
        if unknown:
            parser.error("unknown device %s, the devices are %s" % (", ".join(unknown), ", ".join(DEVICES)))
        only = {names[d.lower()] for d in args.only}

    # the settings that change every build. They win over a device's own defines above, and --list
    # shows them because they are folded in here rather than when a build starts
    overrides = {}
    if args.forcedebug:
        overrides["FORCEDEBUG"] = 1
    if args.forceskin is not None:
        overrides["FORCESKIN"] = args.forceskin
    if args.forcescreenbuffer is not None:
        overrides["SCREENBUFFER"] = args.forcescreenbuffer

    targets = [(device, variant, dict(defines, **overrides)) for device, variant, defines in TARGETS
               if only is None or device in only]
    if args.list:
        for device, variant, defines in targets:
            outs = [o.split()[0] for o in DEVICES[device]["outputs"]]
            print("%-45s %s" % (", ".join(file_name(device, variant, ext) for ext in outs), define_flags(defines)))
        return 0

    os.makedirs(RELEASES, exist_ok=True)
    os.makedirs(WORK, exist_ok=True)
    failed = []
    for device, variant, defines in targets:
        tag = "%s%s" % (device, variant)
        build_dir = os.path.join(WORK, tag)
        log = os.path.join(WORK, tag + ".log")
        shutil.rmtree(build_dir, ignore_errors=True)
        os.makedirs(build_dir)
        print("%-28s %s ..." % (tag, define_flags(defines)), end="", flush=True)
        start = time.time()

        if DEVICES[device].get("cmake"):
            built = build_windows(defines, build_dir, args.msys2, cross, args.lovyangfx, log)
        elif DEVICES[device].get("gba"):
            built = build_gba(defines, build_dir, args.msys2, args.devkitpro, log)
        elif DEVICES[device].get("nds"):
            built = build_nds(defines, build_dir, args.msys2, args.devkitpro, log)
        elif DEVICES[device].get("3ds"):
            built = build_3ds(defines, build_dir, args.msys2, args.devkitpro, log)
        elif DEVICES[device].get("psx"):
            built = build_psx(defines, build_dir, args.msys2, args.psn00bsdk, log)
        elif DEVICES[device].get("n64"):
            built = build_n64(defines, build_dir, args.msys2, args.n64, log)
        elif DEVICES[device].get("web"):
            built = build_web(defines, build_dir, args.msys2, args.emsdk, log)
        elif DEVICES[device].get("dos"):
            built = build_dos(defines, build_dir, args.msys2, args.dosdev, log)
        elif DEVICES[device].get("libretro"):
            built = build_libretro(defines, build_dir, args.msys2, args.libretro_common, cross, log)
        elif DEVICES[device].get("playdate"):
            built = build_playdate(defines, build_dir, args.msys2, args.playdate_sdk, args.playdate_arm, cross, log)
        elif DEVICES[device].get("psp"):
            built = build_psp(defines, build_dir, args.pspdev, log)
        elif DEVICES[device].get("vita"):
            built = build_vita(defines, build_dir, args.msys2, args.vitasdk, log)
        else:
            # the core and libraries are compiled once per device and kept between builds
            cache_dir = os.path.join(WORK, "cache_%s_%s" % (device, cache_tag(device, defines)))
            os.makedirs(cache_dir, exist_ok=True)
            if args.arduino_cli:
                built = build_arduino_cli(device, defines, build_dir, cache_dir, args.arduino_cli, log)
            else:
                built = build_arduino(device, defines, build_dir, cache_dir, args.arduino, log)
        if not built:
            print(" FAILED, see %s" % log)
            for line in errors_in(log):
                print("    " + line)
            failed.append(tag)
            continue

        written = []
        for output in DEVICES[device]["outputs"]:
            ext = output.split()[0]
            target = os.path.join(RELEASES, file_name(device, variant, ext))
            if output == "zip from bin":
                also = [built + e for e in DEVICES[device].get("also", ())]
                package_with_data(built + ".bin", DEVICES[device]["folder"],
                                  DEVICES[device]["data"], target, also)
            elif output == "uf2 from bin":
                with open(built + ".bin", "rb") as f:
                    data = f.read()
                with open(target, "wb") as f:
                    f.write(bin_to_uf2(data, *DEVICES[device]["uf2"]))
            else:
                shutil.copyfile(built + "." + ext, target)
            written.append("%s (%d KB)" % (os.path.basename(target), (os.path.getsize(target) + 1023) // 1024))
        print(" %ds: %s" % (time.time() - start, ", ".join(written)))

    if failed:
        print("\n%d of %d builds failed: %s" % (len(failed), len(targets), ", ".join(failed)))
        return 1
    print("\nall %d builds are in %s" % (len(targets), RELEASES))
    return 0


if __name__ == "__main__":
    sys.exit(main())
