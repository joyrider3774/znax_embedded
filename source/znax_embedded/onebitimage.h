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

//A mask follows the pixels; whether either plane holds its rows as they are rather than run
//length encoded, which is what a picture only a tile wide comes to; and whether either plane is
//one stream over the whole of it rather than one stream a row. See tools/onebit.py
#define ONEBIT_FLAG_MASK 0x01
#define ONEBIT_FLAG_RAW_PIXELS 0x02
#define ONEBIT_FLAG_RAW_MASK 0x04
#define ONEBIT_FLAG_STREAM_PIXELS 0x08
#define ONEBIT_FLAG_STREAM_MASK 0x10
#define ONEBIT_FLAG_ROWS_PIXELS 0x20
#define ONEBIT_FLAG_ROWS_MASK 0x40

//What the header holds. Inline, and deliberately: a sprite draw asks for three of these before it
//starts, so a board full of them made thousands of calls a frame into another file to read a byte
static inline int OneBitWidth(const uint8_t* d) { return PLATFORM_READ_BYTE(d + 2) | (PLATFORM_READ_BYTE(d + 3) << 8); }
static inline int OneBitHeight(const uint8_t* d) { return PLATFORM_READ_BYTE(d + 4) | (PLATFORM_READ_BYTE(d + 5) << 8); }
static inline int OneBitFlags(const uint8_t* d) { return PLATFORM_READ_BYTE(d + 1); }
//where the mask starts, 0 when the picture has nothing to skip and carries none
static inline int OneBitMaskAt(const uint8_t* d) { return PLATFORM_READ_BYTE(d + 6) | (PLATFORM_READ_BYTE(d + 7) << 8); }

//Reads a plane a row at a time. A plane that is one stream has runs carrying on over the end of
//a row, so what is left of one is kept here between rows; a plane encoded a row at a time leaves
//nothing over at a row's end, so the same reader reads both and nothing has to tell them apart
typedef struct OneBitReader OneBitReader;
struct OneBitReader
{
	const uint8_t* pos;   //the next control byte, or the next byte of a literal
	uint16_t left;        //how much of the run or the literal is still to come
	uint8_t again;        //how many more times the row that was read last is the row to give
	uint8_t repeated;     //the byte a run repeats
	bool inRun;
	bool raw;             //the plane is kept as it is, so a row is simply the next stride bytes
	bool rows;            //the plane is rows that may say "the row above, again"
};

//Starts at the first byte of a plane. flags is the picture's flags byte and forMask picks which
//of its bits are read, so that a caller never has to take it apart itself
PLATFORM_HOT_CODE void OneBitReaderInit(OneBitReader* reader, const uint8_t* plane, int flags, bool forMask);
//unpacks the next row of the plane
PLATFORM_FAST_CODE PLATFORM_HOT_CODE void OneBitReaderRow(OneBitReader* reader, uint8_t* row, int stride);
//Passes over whole rows, for the ones above the part being drawn. row is left holding the last of
//them: a plane packed as rows may say that what comes next is one of these again, and then there
//is nothing else to give it from
PLATFORM_FAST_CODE PLATFORM_HOT_CODE void OneBitReaderSkip(OneBitReader* reader, int rows, int stride, uint8_t* row);
//Temporary, see CHGAME_TIMING in Platform.h: how many rows have been unpacked and how many have
//been passed over to reach a frame further down a sheet, and where the time of composing a strip
//goes. The frame report prints them, so that the cost of drawing is split between the pixels and
//the unpacking rather than guessed at. A game that paints whole images rather than strips leaves
//the three below at zero
#if CHGAME_TIMING
extern uint32_t oneBitRowsRead;
extern uint32_t oneBitRowsSkipped;
extern uint32_t bandBgUs;
extern uint32_t bandSpriteUs;
extern uint32_t bandCoverUs;
#endif

//Reads the bit of column c out of an unpacked row. Inline, and deliberately: this is called for
//every pixel of every one bit picture, and as a function of its own in another file it cost a call
//into flash per pixel, which on a device that runs from flash with wait states was most of what
//drawing cost
static inline bool OneBitAt(const uint8_t* row, int c)
{
    return (row[c >> 3] & (0x80 >> (c & 7))) != 0;
}

//Draws the w x h part at sx,sy of a one bit picture at x,y on the screen, clipped to it. With
//transparent set the pixels its mask clears are skipped; a picture with nothing to skip carries
//no mask and is drawn whole whatever the call asks for
PLATFORM_FAST_CODE void drawImageOneBitPart(int x, int y, int sx, int sy, int w, int h,
                                            const uint8_t* data, bool transparent);

#endif

#endif
