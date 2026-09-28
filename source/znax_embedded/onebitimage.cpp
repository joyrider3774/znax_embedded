//The one bit picture format, see onebitimage.h and tools/onebit.py.

#include <string.h>
#include "onebitimage.h"

#if ONEBITIMAGES

int OneBitWidth(const uint8_t* d) { return PLATFORM_READ_BYTE(d + 2) | (PLATFORM_READ_BYTE(d + 3) << 8); }
int OneBitHeight(const uint8_t* d) { return PLATFORM_READ_BYTE(d + 4) | (PLATFORM_READ_BYTE(d + 5) << 8); }
int OneBitMaskAt(const uint8_t* d) { return PLATFORM_READ_BYTE(d + 6) | (PLATFORM_READ_BYTE(d + 7) << 8); }
//reads the bit of column c out of an unpacked row
bool OneBitAt(const uint8_t* row, int c) { return (row[c >> 3] & (0x80 >> (c & 7))) != 0; }

//unpacks one row of a plane and says where the next one starts
PLATFORM_FAST_CODE const uint8_t* OneBitRow(const uint8_t* p, uint8_t* row, int stride)
{
    int done = 0;
    while (done < stride)
    {
        const uint8_t control = PLATFORM_READ_BYTE(p++);
        int n = (control & 0x7F) + 1;
        if (n > stride - done)
            n = stride - done;
        if (control & 0x80)
        {
            memset(row + done, PLATFORM_READ_BYTE(p), n);
            p++;
        }
        else
        {
            PLATFORM_READ_BYTES(row + done, p, n);
            p += n;
        }
        done += n;
    }
    return p;
}

//passes over whole rows without unpacking them, for the rows above the part being drawn. Every
//row is encoded on its own, which is what makes this possible at all
PLATFORM_FAST_CODE const uint8_t* OneBitSkip(const uint8_t* p, int rows, int stride)
{
    while (rows-- > 0)
    {
        int done = 0;
        while (done < stride)
        {
            const uint8_t control = PLATFORM_READ_BYTE(p++);
            const int n = (control & 0x7F) + 1;
            p += (control & 0x80) ? 1 : n;
            done += n;
        }
    }
    return p;
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
    const uint8_t* pixels = OneBitSkip(data + ONEBIT_HEADER, r0, stride);
    const uint8_t* mask = useMask ? OneBitSkip(data + maskAt, r0, stride) : NULL;
    uint8_t rowPixels[ONEBIT_MAX_STRIDE];
    uint8_t rowMask[ONEBIT_MAX_STRIDE];

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
        pixels = OneBitRow(pixels, rowPixels, stride);
        if (mask)
            mask = OneBitRow(mask, rowMask, stride);
        uint8_t* dstRow = dst + (oy + cy) * dstStride;
        int c = c0;
        //the whole bytes in the middle are the picture's own. A full screen background is one
        //memcpy a row and nothing else
        if (!mask && aligned)
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
            if (mask && !OneBitAt(rowMask, c))
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
        pixels = OneBitRow(pixels, rowPixels, stride);
        if (mask)
            mask = OneBitRow(mask, rowMask, stride);
  #if SCREENBUFFER
        for (int c = c0; c < c1; c++)
        {
            if (mask && !OneBitAt(rowMask, c))
                continue;
            SetBufferPixel(dst, (int16_t)(ox + c), (int16_t)(oy + cy),
                           OneBitAt(rowPixels, c) ? ONEBIT_SET : ONEBIT_CLEAR);
        }
  #else
        if (!mask)
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
