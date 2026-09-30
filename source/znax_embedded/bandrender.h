#ifndef BANDRENDER_H
#define BANDRENDER_H

#include <stdint.h>
//for ONEBITIMAGES and FOURBITIMAGES, which say how the skin built in stores its pictures
#include "defines.h"

//Paints part of the screen in one pass, for the devices that have no screen buffer.
//
//Without a buffer every drawing call goes straight to the display, so painting the background
//first and the blocks on top of it means the display really shows the bare background for a
//moment. On a whole row of cells at once that is the rectangle of background that could be seen
//where READY and GO had been, and on one cell it is the flicker under a block that changes.
//Here the picture is put together in memory a strip at a time and every strip is sent in one go.
//No pixel is written twice and nothing half drawn is ever on the display.
//
//A screen paints itself like this:
//
//    if (BandRender_Begin(background, x, y, w, h))
//        while (BandRender_Next())
//            ...the ordinary drawing calls, in the order they should be painted...
//    else
//        ...the ordinary drawing calls, straight to the display...
//
//While a strip is open the drawing calls of helperfuncs.cpp write into it instead of to the
//display, so the game's own drawing code is used unchanged. Text is the one thing that does not
//go into a strip, the font is drawn by the display library itself: print it after the strips,
//where it costs nothing to see because it only puts figures on top of what is already right.
//
//The memory comes from the heap when the game screen starts and goes back when it ends, see
//Main.cpp. Without it Begin gives false and the screen paints the plain way. So does a device
//that draws off screen anyway (PLATFORM_OFFSCREEN_DRAW), where there is no flicker to keep away
//and the strips would only cost work: there none of this is built at all.

void BandRender_Init(void);
void BandRender_Deinit(void);
//false when the memory could not be had, or when this device has a screen buffer and needs none
bool BandRender_Ready(void);

bool BandRender_Begin(const uint8_t* background, int16_t x, int16_t y, int16_t w, int16_t h);
//sends the strip that was drawn into and opens the next one, false when the rectangle is done
bool BandRender_Next(void);

//the strip that is open now, for drawing that can leave out what does not touch it
int16_t BandRender_StripX(void);
int16_t BandRender_StripY(void);
int16_t BandRender_StripW(void);
int16_t BandRender_StripH(void);

//true while a strip is open. helperfuncs.cpp asks this and sends its drawing here
bool BandRender_Drawing(void);
void BandRender_Fill(int x, int y, int w, int h, uint16_t color);
void BandRender_Image(int x, int y, int sx, int sy, int w, int h, const uint8_t* data, int dataWidth);
#if ONEBITIMAGES
//a picture kept one bit a pixel, which is the whole black & white skin and the full screen
//pictures of the four bit one
void BandRender_ImageOneBit(int x, int y, int sx, int sy, int w, int h,
                            const uint8_t* data, bool transparent);
#endif
#if FOURBITIMAGES
//a picture four bits a pixel with a palette of its own, see fourbitimage.h
void BandRender_Image4Bit(int x, int y, int sx, int sy, int w, int h,
                          const uint8_t* data, bool transparent);
#endif
void BandRender_ImageRLE(int x, int y, int sx, int sy, int w, int h, const uint8_t* data,
                         int dataWidth, int dataHeight, bool transparent);

#endif
