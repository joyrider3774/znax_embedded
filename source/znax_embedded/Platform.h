#ifndef PLATFORM_H
#define PLATFORM_H

//Everything the game needs from the device goes through here: the display and where
//drawing goes, the buttons, sound, saving and memory figures. This header only uses
//standard C headers. What can not be written portably comes from the device's own
//header, picked below, and the functions are implemented by the device's own source:
//PlatformESPboy.h and PlatformESPboy.cpp for the ESPboy.

#include <stdint.h>
#include <stddef.h>
#include "defines.h"

// ===========================================================================
// Settings
// ===========================================================================

//SCREENBUFFER, where drawing goes (0, 1, 8 or 16), is set by the device header that
//PlatformDevice.h includes: how much buffer fits depends on the device. PlatformESPboy.h
//describes the modes. A build can also set it itself

//bytes of save storage the game uses, every save and load goes through one block this size
#define PLATFORM_STORAGE_SIZE 512

// ===========================================================================
// The device
//
// PlatformDevice.h picks the device and includes its header, which supplies:
//   PlatformDisplay, PlatformBuffer   the display and screen buffer classes SCREEN and GFX name
//   SCREENBUFFER_PIXELS()             start of the screen buffer's pixels, in the layout
//                                     SetBufferPixel writes
//   PLATFORM_PROGMEM                  marks const data (images, music) kept in flash
//   PLATFORM_READ_BYTE(addr)          reads one byte of such data
//   PLATFORM_READ_WORD(addr)          reads one 16 bit value of such data
//   PLATFORM_READ_BYTES(dst, addr, n) copies n bytes of such data into memory
// ===========================================================================

#include "PlatformDevice.h"

#ifndef SCREENBUFFER
#error "the device header has to define SCREENBUFFER"
#endif

// ===========================================================================
// What the game supplies
// ===========================================================================

//Game_Setup runs once when the device has started, Game_Loop then over and over for as
//long as it runs. The device's source calls them from wherever its program starts
void Game_Setup(void);
void Game_Loop(void);

// ===========================================================================
// Display
// ===========================================================================

//SCREEN is always the display itself. Only what has to reach the display whatever
//SCREENBUFFER is set to uses it: the unbuffered drawing paths and color565.
extern PlatformDisplay& platformDisplay;
#define SCREEN platformDisplay

//Everything that draws goes through GFX, which is either the display itself or the
//off screen buffer SCREENBUFFER picks. This has to be a macro and not a TFT_eSPI
//reference: pushImage is not virtual, so through a reference to the base class it
//would draw straight to the display even when the reference holds the buffer.
#if SCREENBUFFER
extern PlatformBuffer screenBuffer;
#define GFX screenBuffer
#else
#define GFX SCREEN
#endif

#if SCREENBUFFER == 1
//A 1 bpp buffer only knows set and clear, the frame shows them in the colours given to
//Platform_SetBufferColors. The black & white skin is white on black, a pixel is set when
//it is brighter than mid grey
static inline void SetBufferBit(uint8_t* dst, int16_t x, int16_t y, uint16_t color)
{
	//images repeat the same few colours, so the answer for the last colour is kept instead
	//of working out the brightness again for every pixel
	static uint16_t lastColor = 0x0000;
	static bool lastSet = false;
	if (color != lastColor)
	{
		//luminance on a 0-255 scale from the 5, 6 and 5 bit channels
		uint16_t lum = ((((color >> 11) & 0x1F) << 3) * 77 + (((color >> 5) & 0x3F) << 2) * 150 + ((color & 0x1F) << 3) * 29) >> 8;
		lastColor = color;
		lastSet = (lum >= 128);
	}
	//bits are packed most significant first, the same layout TFT_eSprite::drawPixel uses
	uint8_t mask = 0x80 >> (x & 7);
	if (lastSet)
		dst[(x + y * WINDOW_WIDTH) >> 3] |= mask;
	else
		dst[(x + y * WINDOW_WIDTH) >> 3] &= ~mask;
}
#endif

#if SCREENBUFFER
//writes one RGB565 pixel into the buffer in whatever form the buffer keeps it, x and y
//have to be on screen already
static inline void SetBufferPixel(void* dst, int16_t x, int16_t y, uint16_t color)
{
  #if SCREENBUFFER == 16
	//a 16 bpp sprite keeps its pixels byte swapped
	((uint16_t*)dst)[y * WINDOW_WIDTH + x] = (uint16_t)((color >> 8) | (color << 8));
  #elif SCREENBUFFER == 8
	//RGB332, the same conversion both libraries apply to everything else
	((uint8_t*)dst)[y * WINDOW_WIDTH + x] = (uint8_t)(((color & 0xE000) >> 8) | ((color & 0x0700) >> 6) | ((color & 0x0018) >> 3));
  #else
	SetBufferBit((uint8_t*)dst, x, y, color);
  #endif
}
#endif

//starts the device, shows its boot screen and creates the screen buffer
void Platform_Init(const char* appName);

//The colours a 1 bpp buffer shows its set and clear bits in. Does nothing with any other
//SCREENBUFFER setting
void Platform_SetBufferColors(uint16_t setColor, uint16_t clearColor);

//sends the finished frame to the display, without a screen buffer there is nothing to send
void Platform_PresentFrame(void);

// ===========================================================================
// Buttons
// ===========================================================================

#define BUTTON_LEFT   0x01
#define BUTTON_UP     0x02
#define BUTTON_DOWN   0x04
#define BUTTON_RIGHT  0x08
#define BUTTON_A      0x10
#define BUTTON_B      0x20
//the two side buttons
#define BUTTON_L      0x40
#define BUTTON_R      0x80

//the BUTTON_ bits of every button held down right now
uint8_t Platform_GetButtons(void);

// ===========================================================================
// Time, sound and memory
// ===========================================================================

//microseconds since the device started, wraps around after about 71 minutes
uint32_t Platform_Micros(void);

//plays freq Hz for duration ms, a duration of 0 keeps it playing until the next tone
void Platform_PlayTone(uint16_t freq, uint16_t duration);

//stops the tone that is playing
void Platform_StopTone(void);

//free heap in bytes
uint32_t Platform_FreeHeap(void);

//the least stack that has been free since boot, in bytes
uint32_t Platform_FreeStack(void);

//a value to seed srand with that differs from one start to the next
uint32_t Platform_RandomSeed(void);

//printf style message for whoever is debugging, it does not show on the display
#ifdef __GNUC__
void Platform_Log(const char* format, ...) __attribute__((format(printf, 1, 2)));
#else
void Platform_Log(const char* format, ...);
#endif

// ===========================================================================
// Saved data
// ===========================================================================

//Reads or writes length bytes at offset in the save storage, offset + length has to stay
//within PLATFORM_STORAGE_SIZE. A write of bytes that are already stored costs nothing
void Platform_StorageRead(uint16_t offset, uint8_t* data, uint16_t length);
void Platform_StorageWrite(uint16_t offset, const uint8_t* data, uint16_t length);

#endif
