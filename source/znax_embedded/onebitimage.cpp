//The one bit picture format, see onebitimage.h and tools/onebit.py.

#include <string.h>
#include "onebitimage.h"

#if ONEBITIMAGES

//reads the bit of column c out of an unpacked row
#if CHGAME_TIMING
uint32_t oneBitRowsRead = 0;
uint32_t oneBitRowsSkipped = 0;
uint32_t bandBgUs = 0;
uint32_t bandSpriteUs = 0;
uint32_t bandCoverUs = 0;
#endif

PLATFORM_HOT_CODE void OneBitReaderInit(OneBitReader* reader, const uint8_t* plane, int flags, bool forMask)
{
    reader->pos = plane;
    reader->left = 0;
    reader->again = 0;
    reader->repeated = 0;
    reader->inRun = false;
    reader->raw = (flags & (forMask ? ONEBIT_FLAG_RAW_MASK : ONEBIT_FLAG_RAW_PIXELS)) != 0;
    reader->rows = (flags & (forMask ? ONEBIT_FLAG_ROWS_MASK : ONEBIT_FLAG_ROWS_PIXELS)) != 0;
}

//Takes count bytes off the plane, into out when there is one and dropped when there is not. A run
//or a literal reaching past what was asked for is left part used, which is what lets a plane be
//one stream rather than one a row
static PLATFORM_FAST_CODE PLATFORM_HOT_CODE void OneBitTake(OneBitReader* reader, int count, uint8_t* out)
{
    while (count > 0)
    {
        if (reader->left == 0)
        {
            const uint8_t control = PLATFORM_READ_BYTE(reader->pos);
            reader->pos++;
            reader->left = (uint16_t)((control & 0x7F) + 1);
            reader->inRun = (control & 0x80) != 0;
            if (reader->inRun)
            {
                reader->repeated = PLATFORM_READ_BYTE(reader->pos);
                reader->pos++;
            }
        }
        int n = (int)reader->left;
        if (n > count)
            n = count;
        if (reader->inRun)
        {
            if (out)
                memset(out, reader->repeated, n);
        }
        else
        {
            if (out)
                PLATFORM_READ_BYTES(out, reader->pos, n);
            reader->pos += n;
        }
        if (out)
            out += n;
        reader->left = (uint16_t)(reader->left - n);
        count -= n;
    }
}

//unpacks the next row of the plane
PLATFORM_FAST_CODE PLATFORM_HOT_CODE void OneBitReaderRow(OneBitReader* reader, uint8_t* row, int stride)
{
#if CHGAME_TIMING
    oneBitRowsRead++;
#endif
    //a plane only a tile wide is kept as it is, a control byte in front of two bytes costing more
    //than it saves, and then a row is simply the next stride bytes
    if (reader->raw)
    {
        PLATFORM_READ_BYTES(row, reader->pos, stride);
        reader->pos += stride;
        return;
    }

    if (reader->rows)
    {
        //the row that was read last, again: what row holds is already it, so nothing is written
        if (reader->again)
        {
            reader->again--;
            return;
        }
        const uint8_t item = PLATFORM_READ_BYTE(reader->pos);
        reader->pos++;
        if (item)
        {
            //this row and item - 1 more of them are the row before
            reader->again = (uint8_t)(item - 1);
            return;
        }
        //a row of its own follows, run length encoded and ending where it ends
        reader->left = 0;
        OneBitTake(reader, stride, row);
        return;
    }

    OneBitTake(reader, stride, row);
}

//Passes over whole rows, for the ones above the part being drawn. row is left holding the last of
//them, which a plane packed as rows may yet be asked for again
PLATFORM_FAST_CODE PLATFORM_HOT_CODE void OneBitReaderSkip(OneBitReader* reader, int rows, int stride, uint8_t* row)
{
    if (rows <= 0)
        return;
#if CHGAME_TIMING
    oneBitRowsSkipped += (uint32_t)rows;
#endif
    if (reader->raw)
    {
        reader->pos += (size_t)(rows - 1) * stride;
        PLATFORM_READ_BYTES(row, reader->pos, stride);
        reader->pos += stride;
        return;
    }
    if (reader->rows)
    {
        //there is no passing over these without reading them: what a row is may depend on the
        //rows above it
        while (rows--)
            OneBitReaderRow(reader, row, stride);
        return;
    }
    OneBitTake(reader, rows * stride, NULL);
}

//Draws the w x h part at sx,sy of a one bit image at x,y on the screen, clipped to it. With
//transparent set the pixels its mask clears are skipped, and a picture that has nothing to skip
//carries no mask at all
PLATFORM_FAST_CODE void drawImageOneBitPart(int x, int y, int sx, int sy, int w, int h,
                                            const uint8_t* data, bool transparent)
{
    if (!data || (w <= 0) || (h <= 0))
        return;
    const int dataWidth = OneBitWidth(data);
    const int dataHeight = OneBitHeight(data);
    const int maskAt = OneBitMaskAt(data);
    const bool useMask = transparent && (maskAt != 0);
    //how each plane is packed, which the readers work out from it, see tools/onebit.py
    const int flags = OneBitFlags(data);

    //where the image's top left corner lands, and what of it is drawn: the same clipping the
    //RGB565 path does
    const int ox = x - sx;
    const int oy = y - sy;
    int c0 = sx, c1 = sx + w, r0 = sy, r1 = sy + h;
    if (c0 < 0) c0 = 0;
    if (r0 < 0) r0 = 0;
    if (c1 > dataWidth) c1 = dataWidth;
    if (r1 > dataHeight) r1 = dataHeight;
    if (c0 < -ox) c0 = -ox;
    if (r0 < -oy) r0 = -oy;
    if (c1 > WINDOW_WIDTH - ox) c1 = WINDOW_WIDTH - ox;
    if (r1 > WINDOW_HEIGHT - oy) r1 = WINDOW_HEIGHT - oy;
    if ((c0 >= c1) || (r0 >= r1))
        return;

    const int stride = (dataWidth + 7) / 8;
    uint8_t rowPixels[ONEBIT_MAX_STRIDE];
    uint8_t rowMask[ONEBIT_MAX_STRIDE];
    OneBitReader pixels;
    OneBitReaderInit(&pixels, data + ONEBIT_HEADER, flags, false);
    OneBitReaderSkip(&pixels, r0, stride, rowPixels);
    OneBitReader mask;
    if (useMask)
    {
        OneBitReaderInit(&mask, data + maskAt, flags, true);
        OneBitReaderSkip(&mask, r0, stride, rowMask);
    }

#if SCREENBUFFER == 1
    uint8_t* dst = (uint8_t*)SCREENBUFFER_PIXELS();
    if (!dst)
        return;
    //the buffer's rows start on a byte boundary, the screen being a whole number of bytes wide
    const int dstStride = WINDOW_WIDTH / 8;
    //true when a byte of the picture is a byte of the buffer, so the two can be copied
    const bool aligned = ((((ox + c0) ^ c0) & 7) == 0) && ((c0 & 7) == 0);
    for (int cy = r0; cy < r1; cy++)
    {
        OneBitReaderRow(&pixels, rowPixels, stride);
        if (useMask)
            OneBitReaderRow(&mask, rowMask, stride);
        uint8_t* dstRow = dst + (oy + cy) * dstStride;
        int c = c0;
        //the whole bytes in the middle are the picture's own. A full screen background is one
        //memcpy a row and nothing else
        if (!useMask && aligned)
        {
            const int whole = (c1 - c0) >> 3;
            if (whole > 0)
            {
                memcpy(dstRow + ((ox + c0) >> 3), rowPixels + (c0 >> 3), whole);
                c = c0 + (whole << 3);
            }
        }
        for (; c < c1; c++)
        {
            if (useMask && !OneBitAt(rowMask, c))
                continue;
            const int dx = ox + c;
            const uint8_t bit = 0x80 >> (dx & 7);
            if (OneBitAt(rowPixels, c))
                dstRow[dx >> 3] |= bit;
            else
                dstRow[dx >> 3] &= (uint8_t)~bit;
        }
    }
#else
  #if SCREENBUFFER
    void* dst = SCREENBUFFER_PIXELS();
    if (!dst)
        return;
  #else
    //one transaction for the whole part, and one window when nothing is skipped
    uint16_t line[WINDOW_WIDTH];
    SCREEN.startWrite();
    if (!useMask)
        SCREEN.setAddrWindow(ox + c0, oy + r0, c1 - c0, r1 - r0);
  #endif
    for (int cy = r0; cy < r1; cy++)
    {
        OneBitReaderRow(&pixels, rowPixels, stride);
        if (useMask)
            OneBitReaderRow(&mask, rowMask, stride);
  #if SCREENBUFFER
        for (int c = c0; c < c1; c++)
        {
            if (useMask && !OneBitAt(rowMask, c))
                continue;
            SetBufferPixel(dst, (int16_t)(ox + c), (int16_t)(oy + cy),
                           OneBitAt(rowPixels, c) ? ONEBIT_SET : ONEBIT_CLEAR);
        }
  #else
        if (!useMask)
        {
            for (int c = c0; c < c1; c++)
                line[c - c0] = OneBitAt(rowPixels, c) ? ONEBIT_SET : ONEBIT_CLEAR;
    #if LOVYANGFX
            SCREEN.writePixels(line, c1 - c0, true);
    #else
            SCREEN.pushPixels(line, c1 - c0);
    #endif
        }
        else
        {
            //every run of pixels that is not skipped goes out in a window of its own
            int c = c0;
            while (c < c1)
            {
                while ((c < c1) && !OneBitAt(rowMask, c))
                    c++;
                const int start = c;
                while ((c < c1) && OneBitAt(rowMask, c))
                {
                    line[c - start] = OneBitAt(rowPixels, c) ? ONEBIT_SET : ONEBIT_CLEAR;
                    c++;
                }
                if (c == start)
                    continue;
                SCREEN.setAddrWindow(ox + start, oy + cy, c - start, 1);
    #if LOVYANGFX
                SCREEN.writePixels(line, c - start, true);
    #else
                SCREEN.pushPixels(line, c - start);
    #endif
            }
        }
  #endif
    }
  #if !SCREENBUFFER
    SCREEN.endWrite();
  #endif
#endif
}

#endif
