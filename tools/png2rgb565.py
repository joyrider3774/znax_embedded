#!/usr/bin/env python3
"""Convert the skin images to raw RGB565 flash data headers.

Every pixel becomes 2 bytes, RGB565 little endian. Transparency is not taken from the
alpha channel, the menu words use RGB565 0x005F (COLOR_TRANSPARENT) as the transparent key.
The arrays are marked PLATFORM_PROGMEM, the game's Platform.h says what that is.

Usage: python png2rgb565.py [skins_dir] [images_dir]
defaults to ../assets/skins and ../source/znax_embedded/images next to this script
"""
import os
import sys
from PIL import Image

SKIN_PREFIX = {"default": "default", "black_white": "black_white"}


def to_rgb565(path):
    img = Image.open(path).convert("RGB")
    rgb = img.tobytes()
    pixels = [((rgb[i] >> 3) << 11) | ((rgb[i + 1] >> 2) << 5) | (rgb[i + 2] >> 3) for i in range(0, len(rgb), 3)]
    return img.width, img.height, pixels


def write_header(path, source_name, var, width, height, pixels):
    data = bytearray()
    for p in pixels:
        data += p.to_bytes(2, "little")
    lines = [
        "// Generated from: %s" % source_name,
        "// Format: RGB565_LE",
        "// Original size: %dx%d = %d bytes" % (width, height, len(data)),
        "",
        "const uint16_t %s_width = %d;" % (var, width),
        "const uint16_t %s_height = %d;" % (var, height),
        "",
        "const uint8_t %s_data[] PLATFORM_PROGMEM = {" % var,
    ]
    rows = ["    " + ", ".join("0x%02X" % b for b in data[off:off + 16]) for off in range(0, len(data), 16)]
    lines.append(",\n".join(rows))
    lines.append("};")
    with open(path, "w", newline="\n") as f:
        f.write("\n".join(lines) + "\n")


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    skins_dir = sys.argv[1] if len(sys.argv) > 1 else os.path.join(here, "..", "assets", "skins")
    images_dir = sys.argv[2] if len(sys.argv) > 2 else os.path.join(here, "..", "source", "znax_embedded", "images")
    for skin, prefix in SKIN_PREFIX.items():
        os.makedirs(os.path.join(images_dir, skin), exist_ok=True)
        for png in sorted(os.listdir(os.path.join(skins_dir, skin))):
            if not png.endswith(".png"):
                continue
            name = png[:-4].replace("-", "_")
            width, height, pixels = to_rgb565(os.path.join(skins_dir, skin, png))
            out = os.path.join(images_dir, skin, name + "_RGB565_LE.h")
            write_header(out, png, "%s_%s" % (prefix, name), width, height, pixels)
            print("%-12s %-20s %dx%d" % (skin, png, width, height))


if __name__ == "__main__":
    main()
