#!/usr/bin/env python3
"""Convert the full screen skin images to run length encoded RGB565 flash data headers.

The raw RGB565 full screen images (32KB each) take more flash than the smaller devices have,
so they are stored compressed and drawn with drawImageRLE() in helperfuncs.cpp. The menu words
and the ready, go and time over overlays are compressed as well, with drawImageRLETransparent()
their COLOR_TRANSPARENT (0x005F) pixels are skipped.
The arrays are marked PLATFORM_PROGMEM, the game's Platform.h says what that is.

Stream format, repeated until width * height pixels are produced:
  control byte c
    c & 0x80 : run,     (c & 0x7F) + 1 pixels of the one RGB565 LE pixel that follows
    else     : literal, c + 1 RGB565 LE pixels follow

Usage: python png2rle565.py [skins_dir] [images_dir]
defaults to ../assets/skins and ../source/znax_embedded/images next to this script
"""
import os
import sys
from PIL import Image

import onebit
import fourbit

FULLSCREEN_IMAGES = ["background", "highscores", "intro1", "intro2", "titlescreen"]
WORD_IMAGES = ["credits", "credits1", "credits2", "fixedtimer1", "fixedtimer2", "go", "highscores1", "highscores2",
               "play1", "play2", "ready", "relativetimer1", "relativetimer2", "selectgame", "timeover"]
RLE_IMAGES = FULLSCREEN_IMAGES + WORD_IMAGES
SKIN_PREFIX = {"default": "default", "black_white": "black_white", "default_4b": "default_4b"}
#The black & white skin shows two colours, so it is packed one bit a pixel rather than kept as
#RGB565, which is both smaller and quicker to draw, see tools/onebit.py. The pictures keep
#their names, the game picks the routines to draw them with at build time
ONE_BIT_SKINS = {"black_white"}
#Skins whose pictures are written four bits a pixel with a palette of their own, see
#tools/fourbit.py. The art sits in assets/skins like any other skin's, already cut to sixteen
#colours by tools/make_4b_skin.py, so what the device shows is what is in the repository and a
#picture that came out badly can be painted over by hand. The CHGame builds this one, see FORCESKIN
FOUR_BIT_SKINS = {"default_4b"}
#The pictures of a four bit skin that are stored one bit a pixel instead. At four bits a 128x128
#picture is 8232 bytes however little is in it, and there are five of them; the menu words and the
#game type words are lettering, which one bit suits. Their art in assets/skins/default_4b is the
#line art, put there by tools/make_4b_skin.py
FOUR_BIT_ONE_BIT_FROM = {
    #the five full screen pictures, see the note above
    "background", "titlescreen", "intro1", "intro2", "highscores",
    #the menu words and the game type words, in both their states. They are lettering, which one
    #bit suits, and keeping them that way is 2837 bytes of a device that has 50944 in all
    "play1", "play2", "highscores1", "highscores2", "credits1", "credits2", "credits",
    "fixedtimer1", "fixedtimer2", "relativetimer1", "relativetimer2", "selectgame",
}

COLOR_TRANSPARENT = 0x005F
MAX_COUNT = 128


def to_rgb565(path):
    img = Image.open(path).convert("RGB")
    rgb = img.tobytes()
    pixels = [((rgb[i] >> 3) << 11) | ((rgb[i + 1] >> 2) << 5) | (rgb[i + 2] >> 3) for i in range(0, len(rgb), 3)]
    return img.width, img.height, pixels


def rle_encode(pixels):
    out = bytearray()
    literal = []

    def flush_literal():
        while literal:
            chunk = literal[:MAX_COUNT]
            del literal[:MAX_COUNT]
            out.append(len(chunk) - 1)
            for p in chunk:
                out.extend(p.to_bytes(2, "little"))

    i = 0
    while i < len(pixels):
        run = 1
        while i + run < len(pixels) and run < MAX_COUNT and pixels[i + run] == pixels[i]:
            run += 1
        if run >= 2:
            flush_literal()
            out.append(0x80 | (run - 1))
            out += pixels[i].to_bytes(2, "little")
        else:
            literal.append(pixels[i])
        i += run
    flush_literal()
    return bytes(out)


def rle_decode(data, count):
    pixels = []
    i = 0
    while len(pixels) < count:
        c = data[i]
        n = (c & 0x7F) + 1
        i += 1
        if c & 0x80:
            pixels += [int.from_bytes(data[i:i + 2], "little")] * n
            i += 2
        else:
            pixels += [int.from_bytes(data[i + k * 2:i + k * 2 + 2], "little") for k in range(n)]
            i += n * 2
    return pixels


def write_header(path, source_name, var, width, height, data):
    lines = [
        "// Generated from: %s by tools/png2rle565.py" % source_name,
        "// Format: RGB565_LE, run length encoded, draw with drawImageRLE() or drawImageRLETransparent()",
        "// Original size: %dx%d = %d bytes, encoded: %d bytes" % (width, height, width * height * 2, len(data)),
        "",
        "const uint16_t %s_width = %d;" % (var, width),
        "const uint16_t %s_height = %d;" % (var, height),
        "",
        "const uint8_t %s_rle[] PLATFORM_PROGMEM = {" % var,
    ]
    for off in range(0, len(data), 16):
        lines.append("    " + ", ".join("0x%02X" % b for b in data[off:off + 16]) + ",")
    lines.append("};")
    with open(path, "w", newline="\n") as f:
        f.write("\n".join(lines) + "\n")


def convert(src, out, var, skin):
    """Writes the header for one full screen picture into out, in the format its skin is stored
    in. Every caller wants that same choice made, so it is made here and not in each of them."""
    width, height, pixels = to_rgb565(src)
    #A picture of a four bit skin that is kept one bit a pixel, see FOUR_BIT_ONE_BIT_FROM. The
    #choice is made here and not in main(): convert_skins.py calls this straight and would
    #otherwise store every picture of the skin four bits a pixel
    if (skin in FOUR_BIT_SKINS) and (os.path.basename(src)[:-4] in FOUR_BIT_ONE_BIT_FROM):
        skin = "black_white"
    if skin in FOUR_BIT_SKINS:
        #these are drawn whole, so the pixels are packed
        data, worst = fourbit.encode(pixels, width, height, COLOR_TRANSPARENT, True)
        fourbit.write_header(out, os.path.basename(src), var, var + "_rle", width, height, data,
                             "png2rle565.py")
        return width, height, len(data)
    if skin in ONE_BIT_SKINS:
        data = onebit.encode(pixels, width, height, COLOR_TRANSPARENT)
        onebit.write_header(out, os.path.basename(src), var, var + "_rle", width, height, data,
                            "png2rle565.py")
    else:
        data = rle_encode(pixels)
        #never write something that does not decode back to the picture
        assert rle_decode(data, len(pixels)) == pixels, src
        write_header(out, os.path.basename(src), var, width, height, data)
    return width, height, len(data)


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    skins_dir = sys.argv[1] if len(sys.argv) > 1 else os.path.join(here, "..", "assets", "skins")
    images_dir = sys.argv[2] if len(sys.argv) > 2 else os.path.join(here, "..", "source", "znax_embedded", "images")
    for skin, prefix in SKIN_PREFIX.items():
        os.makedirs(os.path.join(images_dir, skin), exist_ok=True)
        for name in RLE_IMAGES:
            out = os.path.join(images_dir, skin, name + "_RLE565.h")
            width, height, encoded = convert(os.path.join(skins_dir, skin, name + ".png"), out,
                                             "%s_%s" % (prefix, name), skin)
            print("%-12s %-12s %6d -> %6d bytes" % (skin, name, width * height * 2, encoded))


if __name__ == "__main__":
    main()
