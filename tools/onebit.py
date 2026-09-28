#!/usr/bin/env python3
"""The one bit a pixel image format the black & white skin is stored in.

The black & white skin only ever shows two colours, so keeping it as RGB565 spends sixteen bits
on a pixel that carries one. This packs eight pixels into a byte and run length encodes those
bytes, which is both far smaller and quicker to draw: a 1 bpp screen buffer has the very same
layout, so a row is blitted with shifts instead of a colour written per pixel.

    byte 0      0x31, says the array is in this format and not RGB565
    byte 1      flags, bit 0 set when a mask follows the pixels
    bytes 2-3   width, little endian
    bytes 4-5   height, little endian
    bytes 6-7   where the mask starts, counted from the start of the array, 0 when there is none
    byte 8 on   the pixel plane, then the mask plane when there is one

A plane holds one encoding per row, of the (width + 7) / 8 bytes of that row, so a row can be
reached without looking at the pixels of the rows above it. Within a row:

    control byte c
      c & 0x80 : run,     (c & 0x7F) + 1 copies of the byte that follows
      else     : literal, c + 1 bytes follow

A set bit is white and a clear bit is black, the most significant bit of a byte being the leftmost
pixel, which is the order SetBufferBit packs a 1 bpp buffer in. In the mask a set bit is a pixel to
draw and a clear bit one to skip, which is what the transparent colour meant.
"""

MAGIC = 0x31
HEADER = 8
MAX_COUNT = 128


def rle_bytes(row):
    """the control scheme the RGB565 streams use, over plain bytes"""
    out = bytearray()
    literal = []

    def flush():
        while literal:
            chunk = literal[:MAX_COUNT]
            del literal[:MAX_COUNT]
            out.append(len(chunk) - 1)
            out.extend(chunk)

    i = 0
    while i < len(row):
        run = 1
        while i + run < len(row) and run < MAX_COUNT and row[i + run] == row[i]:
            run += 1
        if run >= 2:
            flush()
            out.append(0x80 | (run - 1))
            out.append(row[i])
        else:
            literal.append(row[i])
            run = 1
        i += run
    flush()
    return out


def pack_plane(bits, width, height):
    """the rows of one plane, each packed then run length encoded on its own"""
    stride = (width + 7) // 8
    out = bytearray()
    for y in range(height):
        row = bytearray(stride)
        base = y * width
        for x in range(width):
            if bits[base + x]:
                row[x >> 3] |= 0x80 >> (x & 7)
        out += rle_bytes(row)
    return out


def lum(c565):
    """what SetBufferBit in Platform.h makes of the colour, in the same whole numbers"""
    r = ((c565 >> 11) & 0x1F) << 3
    g = ((c565 >> 5) & 0x3F) << 2
    b = (c565 & 0x1F) << 3
    return (r * 77 + g * 150 + b * 29) >> 8


def encode(pixels, width, height, transparent=None):
    """pixels is the picture as RGB565, transparent the colour that means skip the pixel"""
    lit = []
    mask = []
    any_transparent = False
    for c in pixels:
        if transparent is not None and c == transparent:
            lit.append(0)
            mask.append(0)
            any_transparent = True
        else:
            lit.append(1 if lum(c) >= 128 else 0)
            mask.append(1)

    body = pack_plane(lit, width, height)
    out = bytearray(HEADER)
    out[0] = MAGIC
    out[1] = 1 if any_transparent else 0
    out[2] = width & 0xFF
    out[3] = (width >> 8) & 0xFF
    out[4] = height & 0xFF
    out[5] = (height >> 8) & 0xFF
    out += body
    if any_transparent:
        offset = len(out)
        out[6] = offset & 0xFF
        out[7] = (offset >> 8) & 0xFF
        out += pack_plane(mask, width, height)
    return bytes(out)


def write_header(path, source_name, var, array, width, height, data, tool):
    has_mask = data[1] & 1
    lines = [
        "// Generated from: %s by tools/%s" % (source_name, tool),
        "// Format: 1 bit a pixel, packed and run length encoded, see tools/onebit.py.",
        "// Drawn by the 1 bit routines in helperfuncs.cpp, not by the RGB565 ones.",
        "// Original size: %dx%d = %d bytes as RGB565, here %d bytes%s"
        % (width, height, width * height * 2, len(data), ", with a mask" if has_mask else ""),
        "",
        "const uint16_t %s_width = %d;" % (var, width),
        "const uint16_t %s_height = %d;" % (var, height),
        "",
        "const uint8_t %s[] PLATFORM_PROGMEM = {" % array,
    ]
    for off in range(0, len(data), 16):
        lines.append("    " + ", ".join("0x%02X" % b for b in data[off:off + 16]) + ",")
    lines.append("};")
    with open(path, "w", newline="\n") as f:
        f.write("\n".join(lines) + "\n")
