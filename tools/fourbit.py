"""Pictures four bits a pixel, with a palette of sixteen colours of their own.

Between the two the game already has: the black & white skin is one bit a pixel and two colours, the
default skin is RGB565 and sixteen bits a pixel. Four bits is a quarter of the RGB565 and holds the
art this game actually uses: the whole default skin is 59 distinct colours, and every single picture
in it is 40 or fewer, so sixteen per picture is close and the fonts need two.

The layout, little endian throughout to match the RGB565 headers:

    byte 0      MAGIC
    byte 1      flags, see FLAG_TRANSPARENT
    bytes 2-3   width in pixels
    bytes 4-5   height in pixels
    byte 6      how many colours the palette holds, 1 to 16
    byte 7      0, kept for whatever comes next
    bytes 8..   the palette, that many RGB565 colours
    then        the pixels, two to a byte, the left one in the high nibble

The pixels are kept as they are and not run length encoded. A sheet is read by the row a tile starts
at and only unpacked pixels can be entered there, which the fonts here need; and at four bits a
pixel this whole skin is about 12.8 KB, which the device has room for. See SHEET_IMAGES in
png2rgb565.py for what that costs when a packed picture has to be walked instead.

Index 0 means transparent when FLAG_TRANSPARENT is set, so there is no separate mask: the one bit
format needs a second plane for that, and it is the mask that makes a packed picture awkward.
"""

MAGIC = 0x34
HEADER = 8
MAX_COLOURS = 16

FLAG_TRANSPARENT = 0x01
#the pixels are run length encoded, see rle() and the note in encode()
FLAG_RLE = 0x02

MAX_RUN = 128


def rle(data):
    """The pixels packed, in the control scheme the other converters here use: a byte with the top
    bit set and a count in the rest, then the byte to repeat; or a count, then that many bytes as
    they are.

    Only for a picture that is drawn whole. A packed picture has to be read from its first byte, so
    the row a tile starts at can only be reached by decoding everything above it: that is what cost
    waternet 155 ms of a 166 ms frame, see SHEET_IMAGES in png2rgb565.py."""
    out = bytearray()
    literal = []

    def flush():
        while literal:
            chunk = literal[:MAX_RUN]
            del literal[:MAX_RUN]
            out.append(len(chunk) - 1)
            out.extend(chunk)

    i = 0
    while i < len(data):
        run = 1
        while (i + run < len(data)) and (run < MAX_RUN) and (data[i + run] == data[i]):
            run += 1
        if run >= 2:
            flush()
            out.append(0x80 | (run - 1))
            out.append(data[i])
        else:
            literal.append(data[i])
            run = 1
        i += run
    flush()
    return bytes(out)


def rgb(c565):
    """the colour's eight bit channels, the way the display ends up showing them"""
    return ((((c565 >> 11) & 0x1F) << 3), (((c565 >> 5) & 0x3F) << 2), ((c565 & 0x1F) << 3))


def distance(a, b):
    """how far apart two colours are, weighted the way the eye sees them"""
    ar, ag, ab = rgb(a)
    br, bg, bb = rgb(b)
    dr, dg, db = ar - br, ag - bg, ab - bb
    return 2 * dr * dr + 4 * dg * dg + 3 * db * db


def choose_palette(counts, room):
    """The colours to keep, most used first, at most room of them.

    Most used rather than a median cut: the pictures here hold 40 colours at the outside and a few
    of them cover nearly every pixel, so the ones left over are close to one that is kept."""
    return [c for c, _ in counts.most_common(room)]


def nearest(palette, colour, first):
    """the entry of palette, from first on, that comes closest to colour"""
    best, best_at = None, first
    for i in range(first, len(palette)):
        d = distance(palette[i], colour)
        if (best is None) or (d < best):
            best, best_at = d, i
    return best_at


def encode(pixels, width, height, transparent=None, packed=False):
    """pixels is the picture as RGB565, transparent the colour that means skip the pixel.

    packed asks for the pixels run length encoded, which only a picture drawn whole can be: see
    rle(). It is left off for a sheet and for anything drawn a part at a time.

    Returns the bytes, and how far the worst pixel had to move to reach a colour that was kept."""
    import collections
    see_through = (transparent is not None) and (transparent in pixels)
    #index 0 is the transparent one when there is any, so it is not a colour the picture can use
    first = 1 if see_through else 0
    counts = collections.Counter(c for c in pixels if c != transparent)
    palette = choose_palette(counts, MAX_COLOURS - first)
    if see_through:
        palette = [transparent] + palette
    if not palette:
        palette = [0]

    #every colour the picture holds, mapped to the entry it will be drawn in
    index_of = {}
    worst = 0
    for colour in counts:
        at = nearest(palette, colour, first)
        index_of[colour] = at
        worst = max(worst, distance(palette[at], colour))
    if see_through:
        index_of[transparent] = 0

    out = bytearray(HEADER)
    out[0] = MAGIC
    out[1] = FLAG_TRANSPARENT if see_through else 0
    out[2] = width & 0xFF
    out[3] = (width >> 8) & 0xFF
    out[4] = height & 0xFF
    out[5] = (height >> 8) & 0xFF
    out[6] = len(palette)
    for colour in palette:
        out.append(colour & 0xFF)
        out.append((colour >> 8) & 0xFF)

    #two pixels to a byte, the left one high. An odd row still ends on a byte of its own so that
    #every row starts at one, which is what lets a row be reached by multiplying
    stride = (width + 1) // 2
    body = bytearray()
    for row in range(height):
        line = pixels[row * width:(row + 1) * width]
        for at in range(0, stride * 2, 2):
            left = index_of[line[at]] if at < width else 0
            right = index_of[line[at + 1]] if (at + 1) < width else 0
            body.append(((left & 0x0F) << 4) | (right & 0x0F))
    if packed:
        smaller = rle(bytes(body))
        #a picture with nothing to repeat comes out bigger packed, and then it is left alone
        if len(smaller) < len(body):
            out[1] |= FLAG_RLE
            body = smaller
    out += body
    return bytes(out), worst


def write_header(path, source_name, var, array, width, height, data, tool):
    """the .h for one picture, in the same shape the other converters write"""
    colours = data[6]
    lines = [
        "// Auto-generated by tools/%s" % tool,
        "// Source: %s" % source_name,
        "// %s: %dx%d, four bits a pixel, %d colour palette, %d bytes" %
        (var, width, height, colours, len(data)),
        "// See tools/fourbit.py for the layout",
        "",
        "#pragma once",
        "#include <stdint.h>",
        '#include "../../Platform.h"',
        "",
        #the game checks these against the sizes in defines.h, see SKIN_IMAGE_SIZE
        "const uint16_t %s_width = %d;" % (var, width),
        "const uint16_t %s_height = %d;" % (var, height),
        "",
        "const uint8_t %s[] PLATFORM_PROGMEM = {" % array,
    ]
    rows = ["    " + ", ".join("0x%02X" % b for b in data[off:off + 16])
            for off in range(0, len(data), 16)]
    lines.append(",\n".join(rows))
    lines.append("};")
    with open(path, "w", newline="\n") as f:
        f.write("\n".join(lines) + "\n")
