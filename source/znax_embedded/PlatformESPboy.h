#ifndef PLATFORM_ESPBOY_H
#define PLATFORM_ESPBOY_H

//The ESPboy part of Platform.h, included by it. It supplies what Platform.h can not
//write portably, see "The device" there. The functions are in PlatformESPboy.cpp.

#include <Arduino.h>

//display library: 1 = LovyanGFX, 0 = TFT_eSPI
#define LOVYANGFX 1

#ifndef FPSLOCK
#define FPSLOCK 1
#endif

//where drawing goes:
//  0  = no buffer, everything is drawn straight to the display. No RAM needed but the
//       screens clear and redraw themselves in view, so they flicker
//  1  = 1 bpp TFT_eSprite, every pixel is either ColorWhite or ColorBlack, so the black &
//       white skin is always used. 2K of RAM
//  8  = 8 bpp TFT_eSprite, the whole frame is drawn off screen and pushed once. TFT_eSPI
//       stores it as RGB332, so the images lose most of their colours. 16K of RAM
//  16 = 16 bpp TFT_eSprite, same but full colour. 32K of RAM
//a build can still set it itself
#ifndef SCREENBUFFER
#define SCREENBUFFER 16
#endif

//-1 = the default skin, or the black & white one with a 1 bpp buffer, n = only skin n is built in
//and used, see FORCESKIN in defines.h. Only the one skin is ever built in. A build can still set
//it itself
#ifndef FORCESKIN
#define FORCESKIN -1
#endif

#if LOVYANGFX
//only the library here, the panel and its wiring are configured in PlatformESPboy.cpp.
//Its display class adds nothing but a constructor to LGFX_Device, so the game draws
//through the base class
#define LGFX_USE_V1
#include <LovyanGFX.hpp>
typedef lgfx::LGFX_Device PlatformDisplay;
typedef LGFX_Sprite PlatformBuffer;
//start of the sprite's pixels, both libraries lay them out the same way: 16 bpp byte
//swapped RGB565, 8 bpp RGB332, 1 bpp packed most significant bit first
#define SCREENBUFFER_PIXELS() screenBuffer.getBuffer()
#else
#include <TFT_eSPI.h>
typedef TFT_eSPI PlatformDisplay;
typedef TFT_eSprite PlatformBuffer;
#define SCREENBUFFER_PIXELS() screenBuffer.getPointer()
#endif

//Data in flash: the ESP8266 maps it into memory, but that memory only takes 32 bit reads.
//pgm_read_byte / pgm_read_word read it safely whatever the alignment
//flash is mapped, but that memory only takes 32 bit reads, so it goes through the reads below
#define PLATFORM_DIRECT_FLASH 0
#define PLATFORM_PROGMEM PROGMEM
#define PLATFORM_READ_BYTE(addr) pgm_read_byte(addr)
#define PLATFORM_READ_WORD(addr) pgm_read_word(addr)
//a run of bytes at once, memcpy_P reads 32 bits at a time and is far cheaper per byte
#define PLATFORM_READ_BYTES(dst, addr, len) memcpy_P((dst), (addr), (len))

#endif
