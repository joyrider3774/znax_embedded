"""Packs every skin's art into one file for the SD card, for a build with CARDIMAGES on.

The art normally goes into flash as generated headers, and only one skin fits: 50 KB a skin as
RGB565 against the 50944 bytes a CHGame has for everything, which is what the one bit and four bit
formats exist to get around. Read from a card instead there is no such limit, so every skin is
packed here in full RGB565 and the game picks one at run time.

The file is a container of sections, so what else a game wants from the card later - its levels,
say - is another section rather than another file or another format. The game finds a section by
its four letter name, then an entry inside it by multiplying, and reads a part of one by offset.
See cardimages.h for the reading side.

    header, 16 bytes
      0..3    "CARD"
      4       format version
      5       how many sections
      6..7    0
      8..11   the build stamp: what is packed, hashed. The game checks the card it finds against
              the stamp in cardindex.h, so a card from another build is refused rather than drawn
              as rubbish
      12..15  0
    section table, 16 bytes an entry
      0..3    the name, "IMGS" for the art
      4..7    where the section starts in this file
      8..11   how long it is
      12..15  0
    then each section, starting on a four byte boundary

  The "IMGS" section
      0       how many skins
      1       how many pictures each skin has
      2..3    0
      4..     the index, 12 bytes an entry, skin by skin and within a skin in CARD_IMAGES order
                0..3    where the pixels start, counted from the start of this section
                4..5    width
                6..7    height
                8       format, 0 = RGB565 little endian, a row at a time, no packing
                        1 = one RGB565 colour a row, for a picture whose rows are each one
                            colour; the payload is height colours and not width by height
                9..11   0
      then    the pixels, each picture starting on a four byte boundary

An entry's offset is counted from its own section and not from the file, so a section can be moved
or another one put before it without every entry changing.

Nothing is packed or run length encoded. A card read is by offset and the game wants a row or a
strip out of the middle of a picture, which only unpacked pixels allow: the same reason the four
bit sheets are left unpacked, see rle() in fourbit.py.

    python tools/mkcard.py                  writes the card file and cardindex.h
    python tools/mkcard.py --check          says what it would write and changes nothing
    python tools/mkcard.py --out FILE       somewhere other than the default
"""
import argparse
import io
import os
import struct
import sys
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import png2rgb565 as conv  # noqa: E402

ROOT = os.path.join(HERE, "..")


def skins_dir():
    """Which folder of skins the card is packed from.

    A game drawn at two tile sizes keeps the pictures of each in a folder of its own, and the
    smaller set is in assets/skins2: that is the one the devices that read a card are built with
    (IMAGESET 2, see Defines.h), so it is what goes on the card. A game with one set has only
    assets/skins and that is taken. --skins names another folder under assets, for packing a set
    the devices here do not use; it is read before argparse because the skins are worked out as
    this file is read"""
    assets = os.path.join(ROOT, "assets")
    for i, a in enumerate(sys.argv):
        if (a == "--skins") and (i + 1 < len(sys.argv)):
            return os.path.join(assets, sys.argv[i + 1])
    two = os.path.join(assets, "skins2")
    return two if os.path.isdir(two) else os.path.join(assets, "skins")


SKINS_DIR = skins_dir()


def game_dir():
    """the one source/<game>_embedded folder, so this file is the same in every repository"""
    src = os.path.join(ROOT, "source")
    found = [d for d in sorted(os.listdir(src))
             if d.endswith("_embedded") and os.path.isdir(os.path.join(src, d))]
    if len(found) != 1:
        raise SystemExit("FAIL source/ holds %d _embedded folders, expected 1" % len(found))
    return os.path.join(src, found[0])


GAME_DIR = game_dir()
#"formula1_embedded" -> "Formula1"
GAME = os.path.basename(GAME_DIR)[:-len("_embedded")].capitalize()

MAGIC = b"CARD"
VERSION = 1
HEADER = 16
#a section table entry, and an entry of the art section's index
SECTION = 16
ENTRY = 12
FMT_RGB565 = 0
#A picture whose every row is one colour, kept as that colour a row and nothing else. A full
#screen background is 32768 bytes as pixels and 256 bytes like this, and the game fills the row
#instead of reading it: a board that repaints the whole screen was reading the whole background
#off the card every frame. See CardImages_Row
FMT_ROWS = 1
#the sections, by the name the table holds. Levels will be another one of these
SEC_IMAGES = b"IMGS"

#A skin whose folder ends in this is not packed: it is the default art cut to sixteen colours to
#save flash, and a card has no such shortage, so it would only be a worse copy of default
SKIP_SUFFIX = "_4b"


def skins():
    """Every skin folder, default first so it is skin 0, with the name to show on screen"""
    found = [d for d in sorted(os.listdir(SKINS_DIR))
             if os.path.isdir(os.path.join(SKINS_DIR, d)) and not d.endswith(SKIP_SUFFIX)]
    if "default" in found:
        found.remove("default")
        found.insert(0, "default")
    if not found:
        raise SystemExit("FAIL no skin folders in %s" % SKINS_DIR)
    return [(d, d.replace("_", " ").replace("black white", "Black & white").capitalize())
            for d in found]


def images(packed):
    """The pictures, in the order the index holds them. Every skin has to have the same set"""
    sets = {}
    for skin, _ in packed:
        sets[skin] = {p[:-4] for p in os.listdir(os.path.join(SKINS_DIR, skin))
                      if p.endswith(".png")}
    first = packed[0][0]
    for skin, _ in packed[1:]:
        missing = sets[first] - sets[skin]
        extra = sets[skin] - sets[first]
        if missing or extra:
            raise SystemExit("FAIL %s and %s hold different pictures (%s), nothing written"
                             % (first, skin, ", ".join(sorted(missing | extra))))
    return sorted(sets[first])


SKINS = skins()
IMAGES = images(SKINS)


#A picture is only worth keeping as one colour a row when that saves something: a small tile
#that happens to be striped saves a few dozen bytes and would make the game carry the code that
#draws one for nothing. A full screen background saves 32512
ROWS_WORTH = 1024


def row_colours(path):
    """The colour of each row when every row is one colour, and None when they are not, see
    FMT_ROWS. None as well when there is too little in it to be worth the other form"""
    width, height, px = conv.to_rgb565(path)
    rows = [px[y * width] for y in range(height)]
    if not all(px[y * width + x] == rows[y] for y in range(height) for x in range(width)):
        return None
    if (width * height - height) * 2 < ROWS_WORTH:
        return None
    return rows


def any_rows():
    """True when at least one picture is kept as one colour a row, see FMT_ROWS"""
    for skin, _ in SKINS:
        for name in IMAGES:
            if row_colours(os.path.join(SKINS_DIR, skin, name + ".png")) is not None:
                return True
    return False


def ident(name):
    """A file or folder name as the tail of a C identifier. A picture called
    "box-table-16-16" or a skin called "Ti-83" holds characters an identifier cannot, and
    CARD_IMG_BOX-TABLE-16-16 would not compile; anything that is not a letter or a digit
    becomes an underscore. The name itself is left alone - it is what the stamp is taken over
    and what the report prints"""
    out = "".join(c if c.isalnum() else "_" for c in name.upper())
    #an identifier cannot start with a digit
    return out if (out and not out[0].isdigit()) else ("N" + out)


def card_name():
    """The file's name on the card, and the eleven character form a FAT directory entry holds it
    as. CHSd looks a file up by that form, see fat::find, so the name has to be a plain 8.3 one:
    "Puzzleland" does not fit in eight characters and becomes PUZZLELA.DAT"""
    stem = GAME.upper()[:8]
    return stem + ".DAT", stem.ljust(8) + "DAT"


def stamp():
    """The build stamp: what is packed, not what it looks like. A card whose index no longer matches
    the game's own list of names is from another build and is refused.

    The sizes go in as well as the names. A game drawn at two tile sizes has the same pictures
    under the same names in both of its skin folders, so the names alone cannot tell a card packed
    from the wrong one apart, and the game would read 16x16 tiles as 8x8 and draw noise"""
    sizes = []
    for skin, _ in SKINS:
        for name in IMAGES:
            #the form it is kept in belongs in here as well, see FMT_ROWS: the same picture
            #packed the other way is a different thing to read
            path = os.path.join(SKINS_DIR, skin, name + ".png")
            with conv.Image.open(path) as img:
                width, height = img.width, img.height
            flat = row_colours(path) is not None
            sizes.append("%s:%dx%d:%d" % (name, width, height, FMT_ROWS if flat else FMT_RGB565))
    text = ";".join([s for s, _ in SKINS]) + "|" + ";".join(IMAGES) + "|" + ";".join(sizes)
    return zlib.crc32(text.encode("ascii")) & 0xFFFFFFFF


def images_section():
    """The art section's bytes, and a line per picture for the report"""
    pixels = bytearray()
    index = bytearray()
    report = []
    #within the section: its own 4 byte head, then the whole index, then the pixels
    base = 4 + ENTRY * len(SKINS) * len(IMAGES)
    for skin, _ in SKINS:
        for name in IMAGES:
            src = os.path.join(SKINS_DIR, skin, name + ".png")
            if not os.path.exists(src):
                raise SystemExit("FAIL %s has no %s.png, nothing written" % (skin, name))
            width, height, px = conv.to_rgb565(src)
            #A picture whose every row is one colour is kept as those colours and nothing more.
            #It is not a packing scheme: the game draws it by filling, so it never reads it
            rows = row_colours(src)
            flat = rows is not None
            #4 byte aligned so a read lands evenly, which some cores need for a 16 bit access
            while len(pixels) % 4:
                pixels.append(0)
            offset = base + len(pixels)
            for p in (rows if flat else px):
                pixels += struct.pack("<H", p)
            fmt = FMT_ROWS if flat else FMT_RGB565
            index += struct.pack("<IHHBBBB", offset, width, height, fmt, 0, 0, 0)
            report.append("  %-13s %-12s %4dx%-4d %7d B at %7d%s" %
                          (skin, name, width, height,
                           (height if flat else width * height) * 2, offset,
                           "  one colour a row" if flat else ""))
    head = struct.pack("<BBBB", len(SKINS), len(IMAGES), 0, 0)
    return head + bytes(index) + bytes(pixels), report


def pack():
    """Returns the file's bytes and a line per picture for the report.

    Add a section by returning its bytes here: the table and the offsets follow from it"""
    sections = []
    body, report = images_section()
    sections.append((SEC_IMAGES, body))

    out = bytearray(HEADER + SECTION * len(sections))
    out[0:4] = MAGIC
    out[4] = VERSION
    out[5] = len(sections)
    struct.pack_into("<I", out, 8, stamp())
    for i, (name, body) in enumerate(sections):
        while len(out) % 4:
            out.append(0)
        struct.pack_into("<4sIII", out, HEADER + SECTION * i, name, len(out), len(body), 0)
        out += body
    return bytes(out), report


def write_index_header(path):
    """cardindex.h: the names the game uses, and the stamp it checks the card against"""
    lines = [
        "// Auto-generated by tools/mkcard.py. Do not edit: re-run it.",
        "//",
        "// What the card file holds and in what order, for a build with CARDIMAGES on. The layout",
        "// is set out in tools/mkcard.py; this only names the entries so the game can ask for one",
        "// by name and check that the card it found was made by this build.",
        "",
        "#pragma once",
        "#include <stdint.h>",
        "",
        "//the file the game looks for in the card's root",
        '#define CARD_FILE_NAME "%s"' % card_name()[0],
        "",
        "//the same name as a FAT directory entry holds it: eight characters then three, no dot,",
        "//which is the form CHSd's fat::find takes",
        '#define CARD_FILE_83 "%s"' % card_name()[1],
        "",
        "//every skin and picture name, hashed: a card made by another build has another stamp",
        "#define CARD_STAMP 0x%08XUL" % stamp(),
        "",
        "//the sections of the container, by the name its table holds. Levels will be another one",
        '#define CARD_SEC_IMAGES "%s"' % SEC_IMAGES.decode(),
        "",
        "//1 when any picture is kept as one colour a row, see FMT_ROWS. The game builds the",
        "//code that draws one only then: a card without any is a game that need not carry it",
        "#define CARD_HAS_ROWS %d" % (1 if any_rows() else 0),
        "",
        "#define CARD_SKIN_COUNT %d" % len(SKINS),
        "#define CARD_IMAGE_COUNT %d" % len(IMAGES),
        "",
        "//the skins, in the order the index holds them",
        "enum CardSkin : uint8_t",
        "{",
    ]
    for i, (skin, _) in enumerate(SKINS):
        lines.append("\tCARD_SKIN_%s = %d," % (ident(skin), i))
    lines += [
        "};",
        "",
        "//what each skin is called on screen",
        "#define CARD_SKIN_NAMES { %s }" % ", ".join('"%s"' % n for _, n in SKINS),
        "",
        "//the pictures, in the order the index holds them within a skin",
        "enum CardImage : uint8_t",
        "{",
    ]
    for i, name in enumerate(IMAGES):
        lines.append("\tCARD_IMG_%s = %d," % (ident(name), i))
    lines += ["};", ""]
    with io.open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines))


def default_out():
    return os.path.join(ROOT, "releases", card_name()[0])


def main():
    parser = argparse.ArgumentParser(description="Pack the skins into one file for the SD card")
    parser.add_argument("--check", action="store_true", help="report only, write nothing")
    parser.add_argument("--out", default=None, help="where to write the card file")
    parser.add_argument("--skins", default=None,
                        help="the folder under assets/ to pack, when not the one for the devices "
                             "that read a card (see skins_dir)")
    args = parser.parse_args()

    data, report = pack()
    out = args.out or default_out()
    header = os.path.join(GAME_DIR, "cardindex.h")

    print("\n".join(report))
    print()
    print("%-13s %d skins x %d pictures, %d B index, %d B in all (%.1f KB)" %
          ("card file:", len(SKINS), len(IMAGES), ENTRY * len(SKINS) * len(IMAGES),
           len(data), len(data) / 1024.0))
    print("%-13s 0x%08X" % ("stamp:", stamp()))
    if args.check:
        print()
        print("would write %s" % out)
        print("would write %s" % header)
        return 0

    if not os.path.isdir(os.path.dirname(out)):
        os.makedirs(os.path.dirname(out))
    #through a .tmp so a failure cannot leave a half written card file behind
    tmp = out + ".tmp"
    with io.open(tmp, "wb") as f:
        f.write(data)
    os.replace(tmp, out)
    write_index_header(header)
    print()
    print("wrote %s" % out)
    print("wrote %s" % header)
    print()
    print("Copy the card file into the root of a FAT16 or FAT32 card. Copy it to a freshly")
    print("formatted card if the game says the file is fragmented: it is read along its extents,")
    print("and only a few pieces of it are allowed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
