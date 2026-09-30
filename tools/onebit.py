#!/usr/bin/env python3
"""The one bit a pixel image format the black & white skin is stored in.

The black & white skin only ever shows two colours, so keeping it as RGB565 spends sixteen bits
on a pixel that carries one. This packs eight pixels into a byte and run length encodes those
bytes, which is both far smaller and quicker to draw: a 1 bpp screen buffer has the very same
layout, so a row is blitted with shifts instead of a colour written per pixel.

    byte 0      0x31, says the array is in this format and not RGB565
    byte 1      flags: bit 0 a mask follows the pixels
                       bit 1 the pixel plane holds its rows as they are, not run length encoded
                       bit 2 the same for the mask plane
                       bit 3 the pixel plane is one run length encoded stream over the whole of
                             it, rather than one stream a row
                       bit 4 the same for the mask plane
                       bit 5 the pixel plane is rows that may repeat the row above, see pack_rows
                       bit 6 the same for the mask plane
    bytes 2-3   width, little endian
    bytes 4-5   height, little endian
    bytes 6-7   where the mask starts, counted from the start of the array, 0 when there is none
    byte 8 on   the pixel plane, then the mask plane when there is one

A plane holds its rows one after another, each of (width + 7) / 8 bytes. Either the whole plane
is one run length encoded stream, or each row is a stream of its own, in which case a row can be
reached without unpacking the pixels of the rows above it. The bytes are

    control byte c
      c & 0x80 : run,     (c & 0x7F) + 1 copies of the byte that follows
      else     : literal, c + 1 bytes follow

or written as they are, whichever of the three comes to least. One stream over the plane beats a
stream a row because it spends no control byte on each row and a run may carry on over the end of
one; a row at a time is kept for a picture the band renderer skips into, where reaching a row
without unpacking the ones above it is the point. Writing the rows as they are wins on a tile
sheet, which is only as wide as a tile: a twelve pixel sheet is two bytes to a row, and there a
control byte in front of them is a third more than the row itself.

The reader keeps what is left of a run between rows, so a plane encoded either of the two ways
reads back the same and only its size tells them apart.

A set bit is white and a clear bit is black, the most significant bit of a byte being the leftmost
pixel, which is the order SetBufferBit packs a 1 bpp buffer in. In the mask a set bit is a pixel to
draw and a clear bit one to skip, which is what the transparent colour meant.
"""

MAGIC = 0x31
HEADER = 8
MAX_COUNT = 128

FLAG_MASK = 0x01        #a mask plane follows the pixels
FLAG_RAW_PIXELS = 0x02  #the pixel plane is not run length encoded
FLAG_RAW_MASK = 0x04    #nor is the mask plane
FLAG_STREAM_PIXELS = 0x08  #the pixel plane is one stream rather than a row at a time
FLAG_STREAM_MASK = 0x10    #so is the mask plane
FLAG_ROWS_PIXELS = 0x20    #the pixel plane is rows with repeats, see pack_rows()
FLAG_ROWS_MASK = 0x40      #so is the mask plane


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


def packed_rows(bits, width, height):
    """the rows of one plane, packed eight pixels to a byte"""
    stride = (width + 7) // 8
    rows = []
    for y in range(height):
        row = bytearray(stride)
        base = y * width
        for x in range(width):
            if bits[base + x]:
                row[x >> 3] |= 0x80 >> (x & 7)
        rows.append(row)
    return rows


def pack_rows(rows):
    """One plane as rows that may say "the row above, again".

    A run length encoded byte repeats one byte, so a picture of a hundred identical rows spends a
    control byte on every one of them. Here a plane is a list of items:

        0        the row that follows is written out, run length encoded as ever
        1 to 255 the row before again, this many more times

    A poster of flat colour comes down to a handful of bytes this way; a picture whose rows all
    differ pays a byte for each of them and loses to the other ways of packing it, which is what
    pack_plane() is for."""
    out = bytearray()
    i = 0
    while i < len(rows):
        same = 1
        while i + same < len(rows) and rows[i + same] == rows[i] and same < 255:
            same += 1
        out.append(0)
        out += rle_bytes(rows[i])
        if same > 1:
            out.append(same - 1)
        i += same
    return bytes(out)


def pack_plane(bits, width, height, keep_raw=False):
    """One plane, in whichever of the three ways comes to least.

    Its rows run length encoded one at a time, the plane run length encoded as one stream, or the
    rows written as they are. Encoding a row at a time costs a control byte for every row and no
    run can carry on over the end of one, so the whole plane as one stream is usually smaller; a
    plane of two byte rows is smaller again written as it is, a control byte in front of two bytes
    costing more than it saves.

    The reader takes the bytes it is asked for and keeps what is left of a run between rows, so a
    plane encoded either way reads back the same and only the size tells them apart.

    keep_raw asks for the plane as it is whatever that costs. A sheet is read by the row a tile
    starts at, and only a raw plane can be entered there: every other packing has to be decoded from
    the top, so reaching a tile far down a tall sheet costs every row before it. See SHEET_IMAGES.

    Returns the bytes, whether they are raw, and whether they are one stream."""
    rows = packed_rows(bits, width, height)
    if keep_raw:
        plain = bytearray()
        for row in rows:
            plain += row
        return plain, True, False, False
    plain = bytearray()
    for row in rows:
        plain += row
    per_row = bytearray()
    for row in rows:
        per_row += rle_bytes(row)
    stream = rle_bytes(bytes(plain))
    repeats = pack_rows(rows)
    best = min(len(plain), len(per_row), len(stream), len(repeats))
    if len(plain) == best:
        return plain, True, False, False
    if len(repeats) == best:
        return repeats, False, False, True
    if len(stream) == best:
        return stream, False, True, False
    return per_row, False, False, False


def lum(c565):
    """what SetBufferBit in Platform.h makes of the colour, in the same whole numbers"""
    r = ((c565 >> 11) & 0x1F) << 3
    g = ((c565 >> 5) & 0x3F) << 2
    b = (c565 & 0x1F) << 3
    return (r * 77 + g * 150 + b * 29) >> 8


def encode(pixels, width, height, transparent=None, keep_raw=False):
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

    body, raw_pixels, stream_pixels, rows_pixels = pack_plane(lit, width, height, keep_raw)
    flags = FLAG_MASK if any_transparent else 0
    if raw_pixels:
        flags |= FLAG_RAW_PIXELS
    if stream_pixels:
        flags |= FLAG_STREAM_PIXELS
    if rows_pixels:
        flags |= FLAG_ROWS_PIXELS

    out = bytearray(HEADER)
    out[0] = MAGIC
    out[2] = width & 0xFF
    out[3] = (width >> 8) & 0xFF
    out[4] = height & 0xFF
    out[5] = (height >> 8) & 0xFF
    out += body
    if any_transparent:
        offset = len(out)
        out[6] = offset & 0xFF
        out[7] = (offset >> 8) & 0xFF
        #The mask is kept raw for a sheet as well. Leaving it packed looked like a bargain, 60 bytes
        #against 5120, but OneBitReaderSkip has to read a rows packed plane a row at a time to know
        #where it is, so reaching a tile down the sheet cost the same walk the pixels used to: the
        #row decodes went back to 62000 a frame and the frame to 138 ms. Both planes or neither
        mask_body, raw_mask, stream_mask, rows_mask = pack_plane(mask, width, height, keep_raw)
        if raw_mask:
            flags |= FLAG_RAW_MASK
        if stream_mask:
            flags |= FLAG_STREAM_MASK
        if rows_mask:
            flags |= FLAG_ROWS_MASK
        out += mask_body
    out[1] = flags
    return bytes(out)


def write_header(path, source_name, var, array, width, height, data, tool):
    has_mask = data[1] & FLAG_MASK
    how = "rows as they are" if (data[1] & FLAG_RAW_PIXELS) else "rows run length encoded"
    lines = [
        "// Generated from: %s by tools/%s" % (source_name, tool),
        "// Format: 1 bit a pixel, %s, see tools/onebit.py." % how,
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
