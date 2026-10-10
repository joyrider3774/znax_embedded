#ifndef CARDIMAGES_H
#define CARDIMAGES_H

#include <stdint.h>
#include "defines.h"

//The art read from a card instead of built into flash, for a build with CARDIMAGES on.
//
//Only one skin fits in a device's flash, and only in a reduced form: the art as RGB565 is hundreds
//of KB a skin against the 50944 bytes a CHGame has for everything, which is what the one bit and
//four bit formats exist to get around. From a card there is nothing to get around, so every skin
//is on it in full RGB565 and the game picks one while it runs.
//
//The game keeps a picture as a const uint8_t* exactly as it keeps a flash array, so every drawing
//call stays as it was. What changes is one line inside the drawing: where a row's pixels come
//from. The RGB565 paths already put a row together at a time in a scratch of their own, so they
//ask CardImages_Row for it rather than reading it out of flash, and all the clipping and all the
//screen buffer arms above that are untouched.
//
//A small picture is read once and kept whole in an arena of CARDARENA bytes, so drawing it again
//costs nothing; a full screen one is read a row or a strip at a time and never cached, which is
//where nearly all the bytes are. See tools/mkcard.py for what is on the card and in what order.

#if CARDIMAGES

//what is on the card and in what order, written by tools/mkcard.py beside the card file itself.
//Here rather than in cardimages.cpp alone: the game names a picture by its CARD_IMG_ number
#include "cardindex.h"

//false when there is no card, no data file, or the file was not made by this build (CARD_STAMP).
//Slow, so for starting up and for retrying and not for a frame
bool CardImages_Open(void);
bool CardImages_Ready(void);
//what is wrong with the card, for the screen that has to say so. NULL when nothing is
const char* CardImages_Problem(void);

//How many skins the card holds, which one the pictures come from now, and switching to another.
//Switching rebuilds the descriptors and empties the arena; nothing else in the game changes
uint8_t CardImages_SkinCount(void);
uint8_t CardImages_Skin(void);
bool CardImages_UseSkin(uint8_t skin);
//what that skin is called, for a menu
const char* CardImages_SkinName(uint8_t skin);

//Empties the arena, which a screen that draws something else entirely should do: what is in
//it stays until then, so the pictures of the screen before would otherwise keep the room
void CardImages_Reset(void);

//The picture to point imgSomething at, by its CARD_IMG_ number from cardindex.h. NULL when the
//card is not there, and the drawing calls then do nothing, as they do with a NULL flash array
const uint8_t* CardImages_Get(uint8_t image);

//its size, for the checks the game makes against the sizes in defines.h
uint16_t CardImages_Width(const uint8_t* image);
uint16_t CardImages_Height(const uint8_t* image);

//The picture's pixels where they sit in RAM, when it is small enough to be kept whole, and NULL
//otherwise. A caller that already has a fast way of copying rows - the band renderer copies them
//a word at a time - then uses it instead of asking for a row at a time through the call below,
//which is what a flash build does with the array in flash. The pointer is good until the arena
//is emptied, so it is asked for again every time it is used and never kept
const uint8_t* CardImages_Cached(const uint8_t* image);

//Count pixels of row y from column x of the picture, into dst as RGB565.
//False when the read did not come, and the caller then leaves dst as it was rather than drawing
//what happens to be in it. A cached picture answers from the arena, a big one from the card
bool CardImages_Row(const uint8_t* image, int x, int y, int count, uint16_t* dst);

//The same for several rows at once, which is one read when they lie together in the file: a row
//of a picture follows the one above it, so rows x..x+count of the full width are one run of
//bytes. A strip of a full screen background is WINDOW_WIDTH by BANDHEIGHT, so the band renderer
//asks for all of it in one go rather than a row at a time - eight reads became one, and what the
//display showed while a strip was being put together stopped being visible
bool CardImages_Rows(const uint8_t* image, int x, int y, int count, int rows, uint16_t* dst);

//How much of the arena is in use and how often a row had to be read because nothing was cached,
//for the debug overlay. Reading costs a card command, so a count that climbs every frame says
//CARDARENA is too small for what that screen draws
uint16_t CardImages_ArenaUsed(void);
uint32_t CardImages_Reads(void);

#endif
#endif
