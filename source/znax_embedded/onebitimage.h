#ifndef onebitimage_h
#define onebitimage_h

//The black & white skin is packed one bit a pixel rather than kept as RGB565, which is both a
//good deal smaller and quicker to draw, see tools/onebit.py for the format. Everything that reads
//those pictures is here: the game's drawing in helperfuncs.cpp, and on a device that builds its
//frame in strips the band renderer as well, so the two cannot drift apart.
//
//ONEBITIMAGES in defines.h says whether the skin built in is stored this way. Nothing below is
//compiled into a game that is not.

#include <stdint.h>
#include "defines.h"
#include "Platform.h"

#if ONEBITIMAGES

//what a set and a clear bit stand for, which is what the skin was drawn in
#define ONEBIT_SET 0xFFFF
#define ONEBIT_CLEAR 0x0000
//the picture's own size and the mask sit in the first eight bytes
#define ONEBIT_HEADER 8
//no picture is wider than the screen, so no row of one is either
#define ONEBIT_MAX_STRIDE ((WINDOW_WIDTH + 7) / 8)

//what the header holds
int OneBitWidth(const uint8_t* data);
int OneBitHeight(const uint8_t* data);
//where the mask starts, 0 when the picture has nothing to skip and carries none
int OneBitMaskAt(const uint8_t* data);

//unpacks one row of a plane and says where the next one starts
PLATFORM_FAST_CODE const uint8_t* OneBitRow(const uint8_t* p, uint8_t* row, int stride);
//passes over whole rows without unpacking them, for the rows above the part being drawn
PLATFORM_FAST_CODE const uint8_t* OneBitSkip(const uint8_t* p, int rows, int stride);
//reads the bit of column c out of an unpacked row
bool OneBitAt(const uint8_t* row, int c);

//Draws the w x h part at sx,sy of a one bit picture at x,y on the screen, clipped to it. With
//transparent set the pixels its mask clears are skipped; a picture with nothing to skip carries
//no mask and is drawn whole whatever the call asks for
PLATFORM_FAST_CODE void drawImageOneBitPart(int x, int y, int sx, int sy, int w, int h,
                                            const uint8_t* data, bool transparent);

#endif

#endif
