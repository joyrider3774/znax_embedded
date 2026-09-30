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

import onebit

SKIN_PREFIX = {"default": "default", "black_white": "black_white"}
#see png2rle565.py: the black & white skin is packed one bit a pixel
ONE_BIT_SKINS = {"black_white"}
COLOR_TRANSPARENT = 0x005F
#the pictures png2rle565.py owns, which are not written raw as well
RLE_IMAGES = {"background", "highscores", "intro1", "intro2", "titlescreen", "credits", "credits1", "credits2", "fixedtimer1", "fixedtimer2", "go", "highscores1", "highscores2", "play1", "play2", "ready", "relativetimer1", "relativetimer2", "selectgame", "timeover"}


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


#Pictures that are a column of tiles, read by the row a tile starts at, and kept unpacked for it: a
#packed plane can only be read from the top, so drawing a tile far down one costs a decode of every
#row above it. none of this game's pictures are read that way, so the tuple is empty.
#Both planes have to be raw or neither, see pack_plane: passing over a packed mask costs the same
#walk the pixels would
SHEET_IMAGES = ()


def convert(src, out, var, skin, keep_raw=False):
    """Writes the header for one picture into out, in the format its skin is stored in.

    Every caller wants that same choice made, so it is made here and not in each of them."""
    width, height, pixels = to_rgb565(src)
    if skin in ONE_BIT_SKINS:
        data = onebit.encode(pixels, width, height, COLOR_TRANSPARENT, keep_raw)
        onebit.write_header(out, os.path.basename(src), var, var + "_data", width, height, data,
                            "png2rgb565.py")
    else:
        write_header(out, os.path.basename(src), var, width, height, pixels)
    return width, height


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    skins_dir = sys.argv[1] if len(sys.argv) > 1 else os.path.join(here, "..", "assets", "skins")
    images_dir = sys.argv[2] if len(sys.argv) > 2 else os.path.join(here, "..", "source", "znax_embedded", "images")
    for skin, prefix in SKIN_PREFIX.items():
        os.makedirs(os.path.join(images_dir, skin), exist_ok=True)
        for png in sorted(os.listdir(os.path.join(skins_dir, skin))):
            if not png.endswith(".png") or png[:-4] in RLE_IMAGES:
                continue
            name = png[:-4].replace("-", "_")
            out = os.path.join(images_dir, skin, name + "_RGB565_LE.h")
            width, height = convert(os.path.join(skins_dir, skin, png), out,
                                    "%s_%s" % (prefix, name), skin, name in SHEET_IMAGES)
            print("%-12s %-20s %dx%d" % (skin, png, width, height))


if __name__ == "__main__":
    main()
