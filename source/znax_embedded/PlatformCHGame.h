#ifndef PLATFORM_CHGAME_H
#define PLATFORM_CHGAME_H

//The CHGame part of Platform.h, included by it. See "The device" in Platform.h for what this
//supplies, the functions are in PlatformCHGame.cpp.
//
//The CHGame is Kevin Bates' handheld around a WCH CH32X035G8U6: a QingKe V4C (RISC-V, 48 MHz)
//with 62 KB of flash, of which the USB bootloader keeps the first 12 KB and the game gets
//50944 bytes, 20464 bytes of RAM and a 128x128 ST7735S on SPI1. The board package is
//github.com/bateske/CH32SerialBoot, the display numbers below come from its own graphics
//library github.com/bateske/CHGfx, which is what this panel is known to like.
//
//The game's screen is 128x128 and so is the display, so the picture fills it exactly and there
//is no border to keep black, unlike the Gamebuino META this is otherwise modelled on. Neither
//LovyanGFX nor TFT_eSPI builds on this core, so the display class is a small one of its own
//below, offering the LovyanGFX calls the game makes the way LovyanGFX takes them, so the game's
//LOVYANGFX drawing paths work unchanged.

#include <stdint.h>
#include <stddef.h>
#include <string.h>

//the display class offers LovyanGFX's calls, so the game takes its LovyanGFX drawing paths
#define LOVYANGFX 1

//Where drawing goes, the modes are described in PlatformESPboy.h. 20464 bytes of RAM leave room
//for 0 (straight to the display) or a 1 bpp buffer, which is 2048 bytes. An 8 bpp buffer would be
//16384 of the 20464 and leave nothing for the stack, the heap and the SD card, so it is not
//offered. A build can still set this itself
#ifndef SCREENBUFFER
#define SCREENBUFFER 0
#endif
#if (SCREENBUFFER != 0) && (SCREENBUFFER != 1)
#error "the CHGame has the RAM for SCREENBUFFER 0 or 1, an 8 bpp buffer would be 16 KB of its 20 KB"
#endif

//-1 = the default skin, or the black & white one with a 1 bpp buffer, n = only skin n is built in
//and used, see FORCESKIN in defines.h. Only the one skin is ever built in. A build can still set
//it itself
#ifndef FORCESKIN
#define FORCESKIN -1
#endif

//What the game draws with, shared by the display and the screen buffer: rectangles and the
//text of the GLCD font, with the arguments LovyanGFX takes
class PlatformCHGameGFX
{
public:
	virtual ~PlatformCHGameGFX() {}

	//RGB565 from 8 bit channels
	static uint16_t color565(uint8_t r, uint8_t g, uint8_t b)
	{
		return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
	}

	virtual void fillRect(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t color) = 0;
	void drawRect(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t color);

	//a background the same as the text colour is not painted, as in LovyanGFX
	void setTextColor(uint16_t fg, uint16_t bg) { textColor = fg; textBackground = bg; }
	void setTextSize(uint8_t size) { textSize = size ? size : 1; }
	//draws character c of the GLCD font at x,y and returns how far the text moves on.
	//the display draws it in one go, see PlatformCHGameDisplay::drawChar
	virtual size_t drawChar(uint16_t c, int32_t x, int32_t y);

	//a transaction around a group of drawing calls, only the display needs one
	virtual void startWrite(void) {}
	virtual void endWrite(void) {}

protected:
	uint16_t textColor = 0xFFFF;
	uint16_t textBackground = 0x0000;
	uint8_t textSize = 1;
};

//the display itself, which the game's screen fills exactly
class PlatformCHGameDisplay : public PlatformCHGameGFX
{
public:
	void init(void);

	void startWrite(void) override;
	void endWrite(void) override;
	//the area the pixels written next fill, left to right and top to bottom
	void setAddrWindow(int32_t x, int32_t y, int32_t w, int32_t h);
	//swap true: the values are plain RGB565 and are put in display order here. False: they
	//already are in display order
	void writePixels(const uint16_t* data, int32_t length, bool swap = true);
	//length pixels of one RGB565 colour
	void writeColor(uint16_t color, uint32_t length);
	//bytes as they are, for pixels already in display order
	void writeBytes(const uint8_t* data, uint32_t length);
	void fillRect(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t color) override;
	//the whole character as one window instead of a window per run of pixels
	size_t drawChar(uint16_t c, int32_t x, int32_t y) override;

private:
	uint8_t writeDepth = 0;
};

//An off screen buffer of the game's size, 1 bit per pixel, laid out the way LovyanGFX's sprites
//keep them: packed most significant bit first
class PlatformCHGameBuffer : public PlatformCHGameGFX
{
public:
	void setColorDepth(uint8_t bits) { depth = bits; }
	bool createSprite(int32_t w, int32_t h);
	void* getBuffer(void) { return pixels; }
	void fillRect(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t color) override;

private:
	uint8_t* pixels = nullptr;
	uint8_t depth = 1;
};

typedef PlatformCHGameDisplay PlatformDisplay;
typedef PlatformCHGameBuffer PlatformBuffer;
#define SCREENBUFFER_PIXELS() screenBuffer.getBuffer()

//flash is ordinary memory on the CH32X035, it can be read like any other
#define PLATFORM_PROGMEM
#define PLATFORM_READ_BYTE(addr) (*(const uint8_t*)(addr))

//the images are little endian RGB565 like the chip itself, memcpy keeps a read from an odd
//address inside a byte array well defined
static inline uint16_t PlatformCHGame_ReadWord(const void* addr)
{
	uint16_t value;
	memcpy(&value, addr, sizeof(value));
	return value;
}
#define PLATFORM_READ_WORD(addr) PlatformCHGame_ReadWord(addr)
#define PLATFORM_READ_BYTES(dst, addr, len) memcpy((dst), (addr), (len))

#endif
