#include <stdint.h>
#include <stddef.h>
#include "Platform.h"
#include "fourbitimage.h"
#include "common.h"

//See fourbitimage.h for the layout. The drawing is the same shape as the RGB565 path in
//helperfuncs.cpp: a row of RGB565 is put together and then either written into the screen buffer or
//pushed to the display, so only how the row is filled differs. There the row is copied out of the
//picture as it lies; here every byte holds two pixels and each one names a colour in the palette.
//
//Marked PLATFORM_HOT_CODE for the reason the one bit drawing is: on a device that runs from flash
//with wait states this loop is several times slower from there, and it runs once for every pixel

//FourBitReader and FourBitReaderInit are in fourbitimage.h, bandrender.cpp reads the same
//pictures into a strip and uses them too
PLATFORM_FAST_CODE PLATFORM_HOT_CODE void FourBitTake(FourBitReader* reader, int count,
                                                      uint8_t* out)
{
    while (count > 0)
    {
        if (reader->left == 0)
        {
            const uint8_t control = PLATFORM_READ_BYTE(reader->pos);
            reader->pos++;
            reader->inRun = (control & 0x80) != 0;
            reader->left = (uint16_t)((control & 0x7F) + 1);
            if (reader->inRun)
            {
                reader->repeated = PLATFORM_READ_BYTE(reader->pos);
                reader->pos++;
            }
        }
        const int take = (count < (int)reader->left) ? count : (int)reader->left;
        if (reader->inRun)
        {
            for (int i = 0; i < take; i++)
                out[i] = reader->repeated;
        }
        else
        {
            PLATFORM_READ_BYTES(out, reader->pos, take);
            reader->pos += take;
        }
        out += take;
        reader->left = (uint16_t)(reader->left - take);
        count -= take;
    }
}

PLATFORM_FAST_CODE PLATFORM_HOT_CODE void drawImage4BitPart(int x, int y, int sx, int sy,
                                                            int w, int h, const uint8_t* data,
                                                            bool transparent)
{
    if (!data)
        return;
    //index 0 is the transparent one, and only when the picture says it has one
    const bool useKey = transparent && ((FourBitFlags(data) & FOURBIT_FLAG_TRANSPARENT) != 0);
    //the columns and rows of the part that are on screen
    const int c0 = (x < 0) ? -x : 0;
    const int c1 = (x + w > WINDOW_WIDTH) ? WINDOW_WIDTH - x : w;
    const int r0 = (y < 0) ? -y : 0;
    const int r1 = (y + h > WINDOW_HEIGHT) ? WINDOW_HEIGHT - y : h;
    if ((c0 >= c1) || (r0 >= r1))
        return;
    const int cols = c1 - c0;
    const int dx = x + c0;

    //The palette, read out of the picture once rather than a byte at a time per pixel. Sixteen
    //entries at the outside, so it is cheap to hold and every pixel is then a lookup
    const int colours = FourBitColours(data);
    uint16_t palette[16];
    for (int i = 0; i < colours; i++)
        palette[i] = (uint16_t)(PLATFORM_READ_BYTE(data + FOURBIT_HEADER + i * 2)
                              | (PLATFORM_READ_BYTE(data + FOURBIT_HEADER + i * 2 + 1) << 8));
    for (int i = colours; i < 16; i++)
        palette[i] = palette[0];

    const int stride = FourBitStride(data);
    const uint8_t* pixels = data + FourBitPixelsAt(data);
    const bool packed = (FourBitFlags(data) & FOURBIT_FLAG_RLE) != 0;
    uint16_t row[WINDOW_WIDTH];
    //which entry each pixel of the row came from, so the transparent ones can be told apart
    uint8_t entry[WINDOW_WIDTH];
    uint8_t packedRow[(WINDOW_WIDTH + 1) / 2];
    FourBitReader reader;
    if (packed)
    {
        FourBitReaderInit(&reader, pixels);
        //the rows above the first one wanted still have to be read to get past them
        for (int r = 0; r < sy + r0; r++)
            FourBitTake(&reader, stride, packedRow);
    }

#if !SCREENBUFFER
    //straight to the display, all the rows of the part in one write transaction
    SCREEN.startWrite();
  #if LOVYANGFX
    //one window for the whole part, but only when every pixel of it is drawn: a keyed picture
    //opens one per run instead, below
    if (!useKey)
        SCREEN.setAddrWindow(dx, y + r0, cols, r1 - r0);
  #endif
#else
    void* buffer = SCREENBUFFER_PIXELS();
    if (!buffer)
        return;
#endif

    for (int r = r0; r < r1; r++)
    {
        //the picture row this screen row comes from. Kept as it lies, that is reached by
        //multiplying: a row always starts on a byte, so there is nothing to decode to get to it
        const uint8_t* line;
        if (packed)
        {
            FourBitTake(&reader, stride, packedRow);
            line = packedRow;
        }
        else
            line = pixels + (size_t)(sy + r) * stride;
        int column = sx + c0;
        for (int c = 0; c < cols; c++, column++)
        {
            const uint8_t pair = packed ? line[column >> 1] : PLATFORM_READ_BYTE(line + (column >> 1));
            //the left pixel of a byte is the high nibble
            const uint8_t index = (uint8_t)((column & 1) ? (pair & 0x0F) : (pair >> 4));
            entry[c] = index;
            row[c] = palette[index];
        }
#if SCREENBUFFER
        const int dy = y + r;
  #if SCREENBUFFER == 16
        uint16_t* d = &((uint16_t*)buffer)[dy * WINDOW_WIDTH + dx];
        //a 16 bpp sprite keeps its pixels byte swapped
        for (int c = 0; c < cols; c++)
            if (!useKey || entry[c])
                d[c] = (uint16_t)((row[c] >> 8) | (row[c] << 8));
  #elif SCREENBUFFER == 8
        uint8_t* d = &((uint8_t*)buffer)[dy * WINDOW_WIDTH + dx];
        for (int c = 0; c < cols; c++)
            if (!useKey || entry[c])
                d[c] = ToBuffer332(row[c], (int16_t)(dx + c), (int16_t)dy);
  #else
        for (int c = 0; c < cols; c++)
            if (!useKey || entry[c])
                SetBufferBit((uint8_t*)buffer, dx + c, dy, row[c]);
  #endif
#else
        if (!useKey)
        {
  #if LOVYANGFX
            //true: the values are plain RGB565, the library puts them in display order
            SCREEN.writePixels(row, cols, true);
  #else
            GFX.pushImage(dx, y + r, cols, 1, row);
  #endif
        }
        else
        {
            //the run of pixels between the transparent ones, each with a window of its own. The
            //whole part cannot go out in one window when some of it is not drawn
            int c = 0;
            while (c < cols)
            {
                while ((c < cols) && !entry[c])
                    c++;
                const int start = c;
                while ((c < cols) && entry[c])
                    c++;
                if (c == start)
                    continue;
  #if LOVYANGFX
                SCREEN.setAddrWindow(dx + start, y + r, c - start, 1);
                SCREEN.writePixels(row + start, c - start, true);
  #else
                GFX.pushImage(dx + start, y + r, c - start, 1, row + start);
  #endif
            }
        }
#endif
    }

#if !SCREENBUFFER
    SCREEN.endWrite();
#endif
}
