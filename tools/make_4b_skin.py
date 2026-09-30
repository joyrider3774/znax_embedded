"""Writes assets/skins/default_4b from the default skin, with every picture cut to sixteen colours.

The four bit skin used to be worked out at build time from the default skin's art. Keeping it as a
skin folder of its own means what the device shows is what is in the repository: the colours can be
looked at, and a picture that came out badly can be painted over by hand without the next build
undoing it. png2rgb565.py then reads this folder like any other skin.

The colours are cut the same way tools/fourbit.py does, so running this and then the converter gives
the same bytes as before. A picture is written back as ordinary RGB; the converter turns 8 bit
channels into RGB565 by dropping the low bits and fourbit.py puts them back by shifting, so a colour
that has been through both is the colour that started.

    python tools/make_4b_skin.py            writes the folder
    python tools/make_4b_skin.py --check    says what it would write and changes nothing
"""
import collections
import os
import shutil
import sys

from PIL import Image

import fourbit
import png2rgb565 as conv

HERE = os.path.dirname(os.path.abspath(__file__))
SKINS = os.path.join(HERE, "..", "assets", "skins")

#the skin this one is made from, and the skin any picture kept one bit a pixel is taken from
FROM_SKIN = "default"
ONE_BIT_FROM_SKIN = "black_white"
TARGET = "default_4b"


def quantised(path):
    """the picture with its colours cut to what fourbit.encode would keep"""
    width, height, pixels = conv.to_rgb565(path)
    transparent = getattr(conv, "COLOR_TRANSPARENT", None)
    see_through = (transparent is not None) and (transparent in pixels)
    first = 1 if see_through else 0
    counts = collections.Counter(c for c in pixels if c != transparent)
    palette = fourbit.choose_palette(counts, fourbit.MAX_COLOURS - first)
    if see_through:
        palette = [transparent] + palette
    if not palette:
        palette = [0]
    keep = {}
    for colour in counts:
        keep[colour] = palette[fourbit.nearest(palette, colour, first)]
    if see_through:
        keep[transparent] = transparent
    return width, height, [keep[c] for c in pixels], len(palette)


def main():
    check = "--check" in sys.argv
    #the pictures of the four bit skin that are kept one bit a pixel, taken from the line art
    one_bit = getattr(conv, "FOUR_BIT_ONE_BIT_FROM", set())
    out_dir = os.path.join(SKINS, TARGET)
    if not check:
        os.makedirs(out_dir, exist_ok=True)
    for png in sorted(os.listdir(os.path.join(SKINS, FROM_SKIN))):
        if not png.endswith(".png"):
            continue
        name = png[:-4]
        out = os.path.join(out_dir, png)
        if name in one_bit:
            src = os.path.join(SKINS, ONE_BIT_FROM_SKIN, png)
            print("%-16s copied from %s, it stays one bit a pixel" % (png, ONE_BIT_FROM_SKIN))
            if not check:
                shutil.copyfile(src, out)
            continue
        width, height, pixels, colours = quantised(os.path.join(SKINS, FROM_SKIN, png))
        print("%-16s %4dx%-5d cut to %2d colours" % (png, width, height, colours))
        if check:
            continue
        img = Image.new("RGB", (width, height))
        img.putdata([fourbit.rgb(c) for c in pixels])
        img.save(out)
    print()
    print("%s %s" % ("would write" if check else "wrote", out_dir))


if __name__ == "__main__":
    main()
