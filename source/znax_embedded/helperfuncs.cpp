#include <stdint.h>
#include "common.h"
#include "helperfuncs.h"

//A row of an image on its way to the display. Where flash is plain memory an evenly placed
//row is handed over where it lies, otherwise it is copied into the scratch row first. A 16
//bit read needs an even address, a core like the Cortex-M0+ faults on an odd one
static inline const uint16_t* ImageRow(const void* src, uint16_t* scratch, int count)
{
#if PLATFORM_DIRECT_FLASH
    if (((uintptr_t)src & 1) == 0)
        return (const uint16_t*)src;
#endif
    PLATFORM_READ_BYTES((uint8_t*)scratch, src, count * sizeof(uint16_t));
    return scratch;
}

//only the skin FORCESKIN picks is part of the build (a 1 bpp buffer forces the black & white one)
#if FORCESKIN == skinDefault
#include "images/default/background_RLE565.h"
#include "images/default/highscores_RLE565.h"
#include "images/default/intro1_RLE565.h"
#include "images/default/intro2_RLE565.h"
#include "images/default/titlescreen_RLE565.h"
#include "images/default/credits_RLE565.h"
#include "images/default/credits1_RLE565.h"
#include "images/default/credits2_RLE565.h"
#include "images/default/fixedtimer1_RLE565.h"
#include "images/default/fixedtimer2_RLE565.h"
#include "images/default/go_RLE565.h"
#include "images/default/highscores1_RLE565.h"
#include "images/default/highscores2_RLE565.h"
#include "images/default/play1_RLE565.h"
#include "images/default/play2_RLE565.h"
#include "images/default/ready_RLE565.h"
#include "images/default/relativetimer1_RLE565.h"
#include "images/default/relativetimer2_RLE565.h"
#include "images/default/selectgame_RLE565.h"
#include "images/default/timeover_RLE565.h"
#include "images/default/blocks_RGB565_LE.h"
#include "images/default/cursor_RGB565_LE.h"
#endif

#if FORCESKIN == skinBlackWhite
#include "images/black_white/background_RLE565.h"
#include "images/black_white/highscores_RLE565.h"
#include "images/black_white/intro1_RLE565.h"
#include "images/black_white/intro2_RLE565.h"
#include "images/black_white/titlescreen_RLE565.h"
#include "images/black_white/credits_RLE565.h"
#include "images/black_white/credits1_RLE565.h"
#include "images/black_white/credits2_RLE565.h"
#include "images/black_white/fixedtimer1_RLE565.h"
#include "images/black_white/fixedtimer2_RLE565.h"
#include "images/black_white/go_RLE565.h"
#include "images/black_white/highscores1_RLE565.h"
#include "images/black_white/highscores2_RLE565.h"
#include "images/black_white/play1_RLE565.h"
#include "images/black_white/play2_RLE565.h"
#include "images/black_white/ready_RLE565.h"
#include "images/black_white/relativetimer1_RLE565.h"
#include "images/black_white/relativetimer2_RLE565.h"
#include "images/black_white/selectgame_RLE565.h"
#include "images/black_white/timeover_RLE565.h"
#include "images/black_white/blocks_RGB565_LE.h"
#include "images/black_white/cursor_RGB565_LE.h"


#endif

//the game draws the images with the sizes in defines.h, a skin has to keep to them
#if FORCESKIN == skinDefault
#define SKIN_IMAGE(name) default_##name
#else
#define SKIN_IMAGE(name) black_white_##name
#endif
#define SKIN_IMAGE_SIZE(name, w, h) ((SKIN_IMAGE(name##_width) == (w)) && (SKIN_IMAGE(name##_height) == (h)))
static_assert(SKIN_IMAGE_SIZE(background, fullScreenWidth, fullScreenHeight) && SKIN_IMAGE_SIZE(highscores, fullScreenWidth, fullScreenHeight) &&
			  SKIN_IMAGE_SIZE(intro1, fullScreenWidth, fullScreenHeight) && SKIN_IMAGE_SIZE(intro2, fullScreenWidth, fullScreenHeight) &&
			  SKIN_IMAGE_SIZE(titlescreen, fullScreenWidth, fullScreenHeight), "a full screen image of the skin is not 128x128");
static_assert(SKIN_IMAGE_SIZE(credits, creditsWidth, creditsHeight) &&
			  SKIN_IMAGE_SIZE(credits1, menuWordWidth, menuWordHeight) && SKIN_IMAGE_SIZE(credits2, menuWordWidth, menuWordHeight) &&
			  SKIN_IMAGE_SIZE(highscores1, menuWordWidth, menuWordHeight) && SKIN_IMAGE_SIZE(highscores2, menuWordWidth, menuWordHeight) &&
			  SKIN_IMAGE_SIZE(play1, menuWordWidth, menuWordHeight) && SKIN_IMAGE_SIZE(play2, menuWordWidth, menuWordHeight) &&
			  SKIN_IMAGE_SIZE(fixedtimer1, timerWordWidth, timerWordHeight) && SKIN_IMAGE_SIZE(fixedtimer2, timerWordWidth, timerWordHeight) &&
			  SKIN_IMAGE_SIZE(relativetimer1, timerWordWidth, timerWordHeight) && SKIN_IMAGE_SIZE(relativetimer2, timerWordWidth, timerWordHeight) &&
			  SKIN_IMAGE_SIZE(selectgame, selectGameWidth, selectGameHeight) && SKIN_IMAGE_SIZE(go, goWidth, goHeight) &&
			  SKIN_IMAGE_SIZE(ready, readyWidth, readyHeight) && SKIN_IMAGE_SIZE(timeover, timeOverWidth, timeOverHeight),
			  "a menu word or overlay image of the skin does not have the size in defines.h");
static_assert(SKIN_IMAGE_SIZE(blocks, blocksWidth, blocksHeight) && SKIN_IMAGE_SIZE(cursor, cursorWidth, cursorHeight),
			  "the blocks or the cursor image of the skin do not have the size in defines.h");

//the skin in use, the one FORCESKIN builds in
uint8_t currentSkin(void)
{
    return FORCESKIN;
}

void preloadImages(void)
{
    switch(currentSkin())
    {
#if FORCESKIN == skinDefault
        case skinDefault:
            ColorStatusText = SCREEN.color565(255,255,255);
            ColorScoreText = SCREEN.color565(102,115,152);
            ColorScoreTextNew = SCREEN.color565(255,115,152);
            break;
#endif
#if FORCESKIN == skinBlackWhite
        case skinBlackWhite:
            ColorStatusText = SCREEN.color565(255,255,255);
            ColorScoreText = SCREEN.color565(0,0,0);
            ColorScoreTextNew = SCREEN.color565(0,0,0);
            break;
#endif
    }
    imgBackground = SKIN_IMAGE(background_rle);
    imgHighScores = SKIN_IMAGE(highscores_rle);
    imgIntro1 = SKIN_IMAGE(intro1_rle);
    imgIntro2 = SKIN_IMAGE(intro2_rle);
    imgTitleScreen = SKIN_IMAGE(titlescreen_rle);
    imgCredits = SKIN_IMAGE(credits_rle);
    imgCredits1 = SKIN_IMAGE(credits1_rle);
    imgCredits2 = SKIN_IMAGE(credits2_rle);
    imgFixedTimer1 = SKIN_IMAGE(fixedtimer1_rle);
    imgFixedTimer2 = SKIN_IMAGE(fixedtimer2_rle);
    imgGo = SKIN_IMAGE(go_rle);
    imgHighScores1 = SKIN_IMAGE(highscores1_rle);
    imgHighScores2 = SKIN_IMAGE(highscores2_rle);
    imgPlay1 = SKIN_IMAGE(play1_rle);
    imgPlay2 = SKIN_IMAGE(play2_rle);
    imgReady = SKIN_IMAGE(ready_rle);
    imgRelativeTimer1 = SKIN_IMAGE(relativetimer1_rle);
    imgRelativeTimer2 = SKIN_IMAGE(relativetimer2_rle);
    imgSelectGame = SKIN_IMAGE(selectgame_rle);
    imgTimeOver = SKIN_IMAGE(timeover_rle);
    imgBlocks = SKIN_IMAGE(blocks_data);
    imgCursor = SKIN_IMAGE(cursor_data);
}

//Milliseconds since the device started, like Arduino's millis(). Platform_Micros wraps around
//after about 71 minutes, the milliseconds are added up from its steps so they do not. Called at
//least once a frame (Game_Loop does), a step is never long enough to wrap by itself
uint32_t getMillis(void)
{
    static uint32_t lastMicros = 0, leftMicros = 0, millis = 0;
    uint32_t now = Platform_Micros();
    leftMicros += now - lastMicros;
    lastMicros = now;
    millis += leftMicros / 1000;
    leftMicros %= 1000;
    return millis;
}

void fillScreen(uint16_t color)
{
    GFX.fillRect(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, color);
}

void fillRect(int x, int y, int w, int h, uint16_t color)
{
    GFX.fillRect(x, y, w, h, color);
}

void drawRect(int x, int y, int w, int h, uint16_t color)
{
    GFX.drawRect(x, y, w, h, color);
}

//multi line text in the 6x8 GLCD font, a cell of 6x8 per character with the background only
//painted when it differs from the text colour, 6 pixels per char and 9 per line
void printText(int16_t x, int16_t y, const char* str, uint16_t color, uint16_t bg, uint8_t size)
{
	int16_t cursorX = x;
	int16_t cursorY = y;
	if (!str)
		return;
#if LOVYANGFX
	//LovyanGFX's drawChar that takes the colours hands them to the font the other way
	//round, set them as the text colour instead. Its default font is the same 6x8 GLCD
	//font and a background equal to the text colour is left out here as well
	GFX.setTextColor(color, bg);
	GFX.setTextSize(size);
#endif
#if SCREENBUFFER == 0
	//Straight to the display every character would be a write transaction of its own, and
	//the chip select sits on the I/O expander: that is I2C traffic per character. One
	//transaction for the whole text instead. Into a buffer nothing is sent, so nothing to do
	SCREEN.startWrite();
#endif
	while (*str)
	{
		if (*str == '\n')
		{
			cursorY += 9 * size;
			cursorX = x;
			str++;
			continue;
		}
#if LOVYANGFX
		GFX.drawChar((uint8_t)*str, cursorX, cursorY);
#else
		GFX.drawChar(cursorX, cursorY, *str, color, bg, size);
#endif
		cursorX += 6 * size;
		str++;
	}
#if SCREENBUFFER == 0
	SCREEN.endWrite();
#endif
}

#if ONEBITIMAGES
// ===========================================================================
// One bit images
//
// The black & white skin is packed one bit a pixel rather than kept as RGB565, see tools/onebit.py
// for the format. A game built with that skin has none of the RGB565 paths below in it, so the
// flash a second decoder would take is not spent.
//
// It is quicker as well as smaller. A row of a picture is packed the very same way the 1 bpp
// screen buffer keeps its own row, so where the two are in step a row is a memcpy and nothing at
// all is worked out per pixel, which is what a full screen background does.
// ===========================================================================

#define ONEBIT_HEADER 8
//what a set and a clear bit stand for, which is what the skin was drawn in
#define ONEBIT_SET 0xFFFF
#define ONEBIT_CLEAR 0x0000
//no picture is wider than the screen, so no row of one is either
#define ONEBIT_MAX_STRIDE ((WINDOW_WIDTH + 7) / 8)

static inline int OneBitWidth(const uint8_t* d) { return PLATFORM_READ_BYTE(d + 2) | (PLATFORM_READ_BYTE(d + 3) << 8); }
static inline int OneBitHeight(const uint8_t* d) { return PLATFORM_READ_BYTE(d + 4) | (PLATFORM_READ_BYTE(d + 5) << 8); }
static inline int OneBitMaskAt(const uint8_t* d) { return PLATFORM_READ_BYTE(d + 6) | (PLATFORM_READ_BYTE(d + 7) << 8); }
//reads the bit of column c out of an unpacked row
static inline bool OneBitAt(const uint8_t* row, int c) { return (row[c >> 3] & (0x80 >> (c & 7))) != 0; }

//unpacks one row of a plane and says where the next one starts
PLATFORM_FAST_CODE static const uint8_t* OneBitRow(const uint8_t* p, uint8_t* row, int stride)
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
PLATFORM_FAST_CODE static const uint8_t* OneBitSkip(const uint8_t* p, int rows, int stride)
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

//Draws the w x h part at sx,sy of a raw RGB565 little endian image that is dataWidth pixels
//wide, at x,y on the screen and clipped to it. With transparent set its COLOR_TRANSPARENT
//pixels are skipped. Every visible row comes out of flash in one copy: LovyanGFX reads image
//data through plain pointers, but PROGMEM on the ESP8266 is flash that only takes 32 bit
//reads, so the rows are read here with PLATFORM_READ_BYTES
void drawImagePart(int x, int y, int sx, int sy, int w, int h, const uint8_t* data, int dataWidth, bool transparent)
{
#if ONEBITIMAGES
    //the skin's pictures carry their own width, the one passed in is the RGB565 path's
    (void)dataWidth;
    drawImageOneBitPart(x, y, sx, sy, w, h, data, transparent);
#else
    if (!data)
        return;
    //the columns and rows of the part that are on screen
    const int c0 = (x < 0) ? -x : 0;
    const int c1 = (x + w > WINDOW_WIDTH) ? WINDOW_WIDTH - x : w;
    const int r0 = (y < 0) ? -y : 0;
    const int r1 = (y + h > WINDOW_HEIGHT) ? WINDOW_HEIGHT - y : h;
    if ((c0 >= c1) || (r0 >= r1))
        return;
    const int cols = c1 - c0;
    const int dx = x + c0;
    //little endian RGB565 like every device, so the bytes can be copied straight into it
    uint16_t row[WINDOW_WIDTH];
#if SCREENBUFFER
    void* buffer = SCREENBUFFER_PIXELS();
    if (!buffer)
        return;
    for (int r = r0; r < r1; r++)
    {
        const int dy = y + r;
        PLATFORM_READ_BYTES((uint8_t*)row, data + ((sy + r) * dataWidth + sx + c0) * sizeof(uint16_t), cols * sizeof(uint16_t));
  #if SCREENBUFFER == 16
        uint16_t* d = &((uint16_t*)buffer)[dy * WINDOW_WIDTH + dx];
        //a 16 bpp sprite keeps its pixels byte swapped
        for (int c = 0; c < cols; c++)
            if (!transparent || (row[c] != COLOR_TRANSPARENT))
                d[c] = (uint16_t)((row[c] >> 8) | (row[c] << 8));
  #elif SCREENBUFFER == 8
        uint8_t* d = &((uint8_t*)buffer)[dy * WINDOW_WIDTH + dx];
        //RGB332, the same conversion SetBufferPixel does
        for (int c = 0; c < cols; c++)
            if (!transparent || (row[c] != COLOR_TRANSPARENT))
                d[c] = ToBuffer332(row[c], (int16_t)(dx + c), (int16_t)dy);
  #else
        for (int c = 0; c < cols; c++)
            if (!transparent || (row[c] != COLOR_TRANSPARENT))
                SetBufferBit((uint8_t*)buffer, dx + c, dy, row[c]);
  #endif
    }
#else
    //straight to the display. The chip select sits on the I/O expander, every write
    //transaction costs I2C traffic, so all the rows go out in one
    SCREEN.startWrite();
  #if LOVYANGFX
    if (!transparent)
    {
        //one window for the whole part, filled a row at a time
        SCREEN.setAddrWindow(dx, y + r0, cols, r1 - r0);
        for (int r = r0; r < r1; r++)
        {
            const uint16_t* prow = ImageRow(data + ((sy + r) * dataWidth + sx + c0) * sizeof(uint16_t), row, cols);
            //true: the values are plain RGB565, the library puts them in display order
            SCREEN.writePixels(prow, cols, true);
        }
    }
    else
    {
        //every run of opaque pixels on a row goes out as one
        for (int r = r0; r < r1; r++)
        {
            //The runs are gathered at the front of the same row, a run never gets ahead of the
            //pixel being read
            const uint16_t* prow = ImageRow(data + ((sy + r) * dataWidth + sx + c0) * sizeof(uint16_t), row, cols);
            int runX = 0, runLen = 0;
            for (int c = 0; c <= cols; c++)
            {
                uint16_t color = COLOR_TRANSPARENT;
                if (c < cols)
                    color = prow[c];
                //the transparent key (and the end of the row) closes a run
                if (color != COLOR_TRANSPARENT)
                {
                    if (runLen == 0)
                        runX = dx + c;
                    row[runLen++] = color;
                }
                else if (runLen > 0)
                {
                    SCREEN.setAddrWindow(runX, y + r, runLen, 1);
                    SCREEN.writePixels(row, runLen, true);
                    runLen = 0;
                }
            }
        }
    }
  #else
    for (int r = r0; r < r1; r++)
    {
        const uint16_t* prow = ImageRow(data + ((sy + r) * dataWidth + sx + c0) * sizeof(uint16_t), row, cols);
        if (transparent)
            GFX.pushImage(dx, y + r, cols, 1, prow, COLOR_TRANSPARENT);
        else
            GFX.pushImage(dx, y + r, cols, 1, prow);
    }
  #endif
    SCREEN.endWrite();
#endif
#endif
}

void drawImageTransparent(int x, int y, int w, int h, const uint8_t* data)
{
    drawImagePart(x, y, 0, 0, w, h, data, w, true);
}

//Draws the w x h part at sx,sy of a run length encoded RGB565 image made by tools/png2rle565.py
//that is dataWidth x dataHeight, at x,y on the screen and clipped to it. A control byte with the
//top bit set is a run of (c & 0x7F) + 1 times the pixel after it, otherwise c + 1 literal pixels
//follow. The image can only be read from its start, so it is decoded up to the last row of the
//part and only the pixels of the part are drawn. With transparent set its COLOR_TRANSPARENT pixels
//are skipped. The data is read with PLATFORM_READ_BYTE and PLATFORM_READ_BYTES: LovyanGFX reads
//image data through plain pointers, but PROGMEM on the ESP8266 is flash that only takes 32 bit reads
void drawImageRLEPart(int x, int y, int sx, int sy, int w, int h, const uint8_t* data, int dataWidth, int dataHeight, bool transparent)
{
#if ONEBITIMAGES
    //the skin's pictures carry their own size, the ones passed in are the RGB565 path's
    (void)dataWidth;
    (void)dataHeight;
    drawImageOneBitPart(x, y, sx, sy, w, h, data, transparent);
#else
    if (!data || (w <= 0) || (h <= 0))
        return;
    //where the image's top left corner lands on the screen
    const int ox = x - sx;
    const int oy = y - sy;
    //the columns and rows of the image that are drawn: the part, cut to the image and the screen
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
#if SCREENBUFFER
    void* dst = SCREENBUFFER_PIXELS();
    if (!dst)
        return;
#else
    //the chip select sits on the I/O expander, so the whole part goes out in one transaction
    //and, without transparency, one window. LovyanGFX's pushBlock and pushPixels open one of
    //their own per call, there the write variants are used inside this one
    SCREEN.startWrite();
    if (!transparent)
        SCREEN.setAddrWindow(ox + c0, oy + r0, c1 - c0, r1 - r0);
#endif
    //a control covers at most 128 pixels
    uint16_t pixels[128];
    uint32_t left = (uint32_t)dataWidth * dataHeight;
    int cx = 0, cy = 0;
    //decoding stops after the last row of the part
    while ((left > 0) && (cy < r1))
    {
        uint8_t control = PLATFORM_READ_BYTE(data++);
        uint16_t count = (control & 0x7F) + 1;
        if (count > left)
            count = (uint16_t)left;
        const bool run = (control & 0x80) != 0;
        uint16_t color = 0;
        if (run)
        {
            color = PLATFORM_READ_BYTE(data) | (PLATFORM_READ_BYTE(data + 1) << 8);
            data += 2;
        }
        else
        {
            //pixels that all lie in rows above the part are only skipped, a control that
            //reaches into the part is read
            if ((uint32_t)(cy * dataWidth + cx + count - 1) / dataWidth >= (uint32_t)r0)
                PLATFORM_READ_BYTES((uint8_t*)pixels, data, count * sizeof(uint16_t));
            data += count * sizeof(uint16_t);
        }
        left -= count;
        //the pixels of the control a row at a time, only the part is drawn
        for (uint16_t done = 0; done < count; )
        {
            int n = dataWidth - cx;
            if (n > count - done)
                n = count - done;
            if ((cy >= r0) && (cy < r1))
            {
                const int a = (cx > c0) ? cx : c0;
                const int e = (cx + n < c1) ? cx + n : c1;
                if (a < e)
                {
                    const uint16_t* src = &pixels[done + (a - cx)];
#if SCREENBUFFER
                    const int sy2 = oy + cy;
                    for (int c = a; c < e; c++)
                    {
                        const uint16_t pixel = run ? color : src[c - a];
                        if (!transparent || (pixel != COLOR_TRANSPARENT))
                            SetBufferPixel(dst, ox + c, sy2, pixel);
                    }
#else
                    //without transparency the pixels fill the one window in order, with it every
                    //run of opaque pixels goes out in a window of its own
                    int c = a;
                    while (c < e)
                    {
                        int start = c;
                        if (transparent)
                        {
                            while ((c < e) && ((run ? color : src[c - a]) == COLOR_TRANSPARENT))
                                c++;
                            start = c;
                            while ((c < e) && ((run ? color : src[c - a]) != COLOR_TRANSPARENT))
                                c++;
                            if (start == c)
                                continue;
                            SCREEN.setAddrWindow(ox + start, oy + cy, c - start, 1);
                        }
                        else
                            c = e;
  #if LOVYANGFX
                        //a uint16_t colour is taken as plain RGB565, true: so are the pixels
                        if (run)
                            SCREEN.writeColor(color, c - start);
                        else
                            SCREEN.writePixels(&src[start - a], c - start, true);
  #else
                        if (run)
                            SCREEN.pushBlock(color, c - start);
                        else
                            SCREEN.pushPixels((uint16_t*)&src[start - a], c - start);
  #endif
                    }
#endif
                }
            }
            done += n;
            cx += n;
            if (cx == dataWidth)
            {
                cx = 0;
                cy++;
            }
        }
    }
#if !SCREENBUFFER
    SCREEN.endWrite();
#endif
#endif
}

void drawImageRLE(int x, int y, int w, int h, const uint8_t* data)
{
    drawImageRLEPart(x, y, 0, 0, w, h, data, w, h, false);
}

void drawImageRLETransparent(int x, int y, int w, int h, const uint8_t* data)
{
    drawImageRLEPart(x, y, 0, 0, w, h, data, w, h, true);
}

//the game's background is as big as the screen, so the part at x,y is the part that belongs there
void drawBackgroundPart(int x, int y, int w, int h)
{
    drawImageRLEPart(x, y, x, y, w, h, imgBackground, fullScreenWidth, fullScreenHeight, false);
}
