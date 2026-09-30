#ifndef FOURBITIMAGE_H
#define FOURBITIMAGE_H

#include <stdint.h>
#include "Platform.h"

//Pictures four bits a pixel with a palette of their own, written by tools/fourbit.py. The layout is
//set out there; what matters here is that the header is 8 bytes, the palette follows it as
//OneBitAt-style little endian RGB565, and the pixels come after that two to a byte with the left
//one in the high nibble. A row always starts on a byte, so the row a tile begins at is reached by
//multiplying rather than by decoding what came before it, which is what a font sheet needs

#define FOURBIT_MAGIC 0x34
#define FOURBIT_HEADER 8
#define FOURBIT_FLAG_TRANSPARENT 0x01
//the pixels are run length encoded. Only a picture drawn whole is packed that way: a packed one can
//only be read from its first byte, so a sheet and anything drawn a part at a time is left as it is
#define FOURBIT_FLAG_RLE 0x02

//what the header holds. Inline for the same reason OneBitWidth is: a draw asks for these before it
//starts, so as calls in another file they cost thousands of them a frame to read a byte
static inline int FourBitFlags(const uint8_t* d) { return PLATFORM_READ_BYTE(d + 1); }
static inline int FourBitWidth(const uint8_t* d) { return PLATFORM_READ_BYTE(d + 2) | (PLATFORM_READ_BYTE(d + 3) << 8); }
static inline int FourBitHeight(const uint8_t* d) { return PLATFORM_READ_BYTE(d + 4) | (PLATFORM_READ_BYTE(d + 5) << 8); }
static inline int FourBitColours(const uint8_t* d) { return PLATFORM_READ_BYTE(d + 6); }
//where the pixels start, past the header and the palette
static inline int FourBitPixelsAt(const uint8_t* d) { return FOURBIT_HEADER + FourBitColours(d) * 2; }
//a row is this many bytes, two pixels to each
static inline int FourBitStride(const uint8_t* d) { return (FourBitWidth(d) + 1) / 2; }

//The pixels of one row of a packed picture, carrying on from where the row before it stopped.
//A packed picture can only be read from its first byte, so a reader walks every row up to the one
//wanted; a picture drawn a part at a time is left unpacked instead, see rle() in tools/fourbit.py
//Shared so bandrender.cpp can read the same pictures into a strip without a second copy of this
typedef struct FourBitReader FourBitReader;
struct FourBitReader
{
    const uint8_t* pos;   //the next control byte, or the next byte of a literal
    uint16_t left;        //how much of the run or the literal is still to come
    uint8_t repeated;     //the byte a run repeats
    bool inRun;
};

//starts a reader at the first byte of the pixels, which is where a packed picture has to start
static inline void FourBitReaderInit(FourBitReader* reader, const uint8_t* pixels)
{
    reader->pos = pixels;
    reader->left = 0;
    reader->repeated = 0;
    reader->inRun = false;
}

//the next count bytes of the stream, which for a whole row is FourBitStride of them
PLATFORM_FAST_CODE PLATFORM_HOT_CODE void FourBitTake(FourBitReader* reader, int count,
                                                      uint8_t* out);

//Draws the w by h part at sx,sy of a four bit picture at x,y on the screen, clipped to it. The
//picture carries its own width, so there is no dataWidth to pass
//With transparent set the pixels whose palette entry is 0 are skipped, which is what index 0 is
//reserved for when FOURBIT_FLAG_TRANSPARENT is set. A picture that carries no transparent colour
//is drawn whole whatever the call asks for
PLATFORM_FAST_CODE PLATFORM_HOT_CODE void drawImage4BitPart(int x, int y, int sx, int sy,
                                                            int w, int h, const uint8_t* data,
                                                            bool transparent);

#endif
