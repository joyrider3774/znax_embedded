#!/usr/bin/env python3
"""Convert the skin images in assets to the flash data headers the game includes.

  assets/skins  -> source/znax_embedded/images

The full screen images, the menu words and the overlays become run length encoded headers
(<name>_RLE565.h, see png2rle565.py), the blocks and the cursor raw RGB565 little endian headers
(<name>_RGB565_LE.h, see png2rgb565.py): the game draws a tile out of the middle of the blocks.

Usage:
  python convert_skins.py            convert every skin, overwriting the headers
  python convert_skins.py --verify   convert into a temporary folder and compare with
                                     the headers that are there now, nothing is written
"""
import argparse
import filecmp
import os
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import png2rle565  # noqa: E402
import png2rgb565  # noqa: E402

ROOT = os.path.join(HERE, "..")
SETS = {
    "images": os.path.join(ROOT, "assets", "skins"),
}
SOURCE_DIR = os.path.join(ROOT, "source", "znax_embedded")

# every skin has to have these, the game includes all of them
IMAGES = png2rle565.RLE_IMAGES + ["blocks", "cursor"]


def header_name(png):
    """File name of the header a skin image converts to."""
    name = png[:-4]
    if name in png2rle565.RLE_IMAGES:
        return name + "_RLE565.h"
    return name + "_RGB565_LE.h"


def convert_image(skins_dir, skin, png, out_dir):
    """Writes the header for one skin image into out_dir/skin, returns its file name."""
    prefix = png2rgb565.SKIN_PREFIX[skin]
    src = os.path.join(skins_dir, skin, png)
    header = header_name(png)
    out = os.path.join(out_dir, skin, header)
    name = png[:-4]
    if header.endswith("_RLE565.h"):
        width, height, pixels = png2rle565.to_rgb565(src)
        data = png2rle565.rle_encode(pixels)
        # never write something that does not decode back to the image
        assert png2rle565.rle_decode(data, len(pixels)) == pixels, src
        png2rle565.write_header(out, png, "%s_%s" % (prefix, name), width, height, data)
    else:
        width, height, pixels = png2rgb565.to_rgb565(src)
        png2rgb565.write_header(out, png, "%s_%s" % (prefix, name.replace("-", "_")), width, height, pixels)
    return header


def convert_set(skins_dir, out_dir):
    """Converts every skin image of one set, returns {skin/header: source png} and missing pngs."""
    made = {}
    missing = []
    for skin in png2rgb565.SKIN_PREFIX:
        os.makedirs(os.path.join(out_dir, skin), exist_ok=True)
        pngs = sorted(f for f in os.listdir(os.path.join(skins_dir, skin)) if f.lower().endswith(".png"))
        missing += ["%s/%s.png" % (skin, name) for name in IMAGES if name + ".png" not in pngs]
        for png in pngs:
            header = convert_image(skins_dir, skin, png, out_dir)
            made["%s/%s" % (skin, header)] = "%s/%s" % (skin, png)
    return made, missing


def existing_headers(images_dir):
    found = set()
    for skin in png2rgb565.SKIN_PREFIX:
        folder = os.path.join(images_dir, skin)
        if os.path.isdir(folder):
            found |= {"%s/%s" % (skin, f) for f in os.listdir(folder) if f.endswith(".h")}
    return found


def verify_set(name, skins_dir, images_dir):
    """True when converting skins_dir gives exactly the headers in images_dir."""
    with tempfile.TemporaryDirectory() as tmp:
        made, missing_pngs = convert_set(skins_dir, tmp)
        have = existing_headers(images_dir)
        same, different, missing = [], [], []
        for header in sorted(made):
            current = os.path.join(images_dir, header)
            if not os.path.exists(current):
                missing.append(header)
            elif filecmp.cmp(os.path.join(tmp, header), current, shallow=False):
                same.append(header)
            else:
                different.append(header)
        extra = sorted(have - set(made))

    print("%s: %d headers checked, %d identical" % (name, len(made), len(same)))
    for header in different:
        print("  DIFFERS  %s (from %s)" % (header, made[header]))
    for header in missing:
        print("  MISSING  %s (would be made from %s)" % (header, made[header]))
    for header in extra:
        print("  EXTRA    %s (in %s but made from no image)" % (header, os.path.basename(images_dir)))
    for png in missing_pngs:
        print("  NO IMAGE %s (the game needs it)" % png)
    return not (different or missing or extra or missing_pngs)


def main():
    parser = argparse.ArgumentParser(description="Convert assets/skins to the image headers")
    parser.add_argument("--verify", action="store_true", help="compare with the current headers, write nothing")
    args = parser.parse_args()

    ok = True
    for name in list(SETS):
        skins_dir = SETS[name]
        images_dir = os.path.join(SOURCE_DIR, name)
        if args.verify:
            ok &= verify_set(name, skins_dir, images_dir)
        else:
            made, missing_pngs = convert_set(skins_dir, images_dir)
            print("%s: wrote %d headers from %s" % (name, len(made), os.path.relpath(skins_dir, ROOT)))
            for png in missing_pngs:
                print("  NO IMAGE %s (the game needs it)" % png)
                ok = False
    if args.verify:
        print("everything matches" if ok else "there are differences")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
