#include <stdint.h>
#include <string.h>
#include "defines.h"
#include "Platform.h"
#include "cardimages.h"

#if CARDIMAGES

#if !PLATFORM_HAS_CARD
#error "CARDIMAGES needs a device that can read a card, see PLATFORM_HAS_CARD in Platform.h"
#endif

#include "cardindex.h"

//The container's layout, set out in tools/mkcard.py. Read a field at a time rather than through a
//struct: the file is little endian whatever the device is, and nothing here may depend on how a
//compiler lays a struct out
#define CARD_HEAD 16
#define CARD_SECTION 16
#define CARD_ENTRY 12
#define CARD_FMT_RGB565 0
//A picture whose every row is one colour, kept as that colour a row and nothing more: 256 bytes
//for a full screen background instead of 32768. The drawing fills the row rather than reading
//it, which is what a board that repaints the whole screen every frame needs. See tools/mkcard.py
#define CARD_FMT_ROWS 1

//A picture of the skin in use. The game holds these as const uint8_t*, so the drawing calls keep
//the signatures they have; CardImages_Row casts back
struct CardEntry
{
	uint32_t at;        //where its pixels start in the file
	uint16_t w, h;
	int16_t slot;       //where in the arena it is kept, or -1
	uint8_t fmt;        //CARD_FMT_, which says what lies at "at" and how much of it
};

//how many bytes of the file a picture takes, which is not w by h for every format
static uint32_t EntryBytes(const CardEntry* img)
{
#if CARD_HAS_ROWS
	if (img->fmt == CARD_FMT_ROWS)
		return (uint32_t)img->h * 2;
#endif
	return (uint32_t)img->w * img->h * 2;
}

static CardEntry images[CARD_IMAGE_COUNT];
static bool ready = false;
static const char* problem = NULL;
static uint8_t skin = 0;
//where the art section starts, so an entry's own offset can be added to it
static uint32_t section = 0;

//The arena. A picture read once is kept here and drawing it again is a copy, which is what makes
//a tile drawn eighty times a frame affordable.
//A bump allocator that is emptied whole when the next picture will not fit, rather than a free
//list: there is no fragmentation to manage and no bookkeeping per picture, and the screens here
//want a few hundred bytes at a time against the CARDARENA they are given. A screen whose pictures
//do not all fit empties it more than once a frame and reads them again, which is correct but
//costs card commands: CardImages_Reads() says how often, and the answer is a larger CARDARENA
//CARDARENA 0 is no arena at all: every row is read off the card as it is drawn. That is what a
//device with no RAM to spare asks for, and it is the arena that has to give way - the game's own
//level does not fit otherwise, and a level that will not load is worse than a slower one
#if CARDARENA > 0
//four byte aligned, and every picture in it starts on a four byte boundary: the band renderer
//copies rows out of here a word at a time, and a core like this one wants them placed for it
static uint8_t arena[CARDARENA] __attribute__((aligned(4)));
#endif
static uint16_t arenaUsed = 0;
static uint32_t reads = 0;

//The biggest picture worth keeping. Anything that fits is kept: the one that matters is whatever
//the game draws over and over, and for a tile game that is a sheet of every tile, which is large
//next to the single tiles around it. Holding back at a third of the arena left znax's 4480 byte
//block sheet uncached, and its board then cost a card read per row per block - about 1350 of them
//to put one board up, which is slower than drawing it at all.
//A picture too big for the arena is read a row at a time instead and never kept, which is what
//the full screen ones want anyway. See CARDARENA, which a game sets to suit its own art
#define CARD_CACHE_MAX CARDARENA

static inline uint16_t Read16(const uint8_t* p)
{
	return (uint16_t)(p[0] | (p[1] << 8));
}

static inline uint32_t Read32(const uint8_t* p)
{
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

bool CardImages_Ready(void) { return ready; }
const char* CardImages_Problem(void) { return problem; }
uint8_t CardImages_Skin(void) { return skin; }
uint8_t CardImages_SkinCount(void) { return CARD_SKIN_COUNT; }
uint16_t CardImages_ArenaUsed(void) { return arenaUsed; }
uint32_t CardImages_Reads(void) { return reads; }

const char* CardImages_SkinName(uint8_t which)
{
	static const char* names[] = CARD_SKIN_NAMES;
	return (which < CARD_SKIN_COUNT) ? names[which] : "";
}

bool CardImages_Open(void)
{
	ready = false;
	problem = NULL;
	arenaUsed = 0;
	reads = 0;

	if (!Platform_CardOpen(CARD_FILE_NAME, CARD_FILE_83))
	{
		problem = "NO CARD DATA";
		return false;
	}

	uint8_t head[CARD_HEAD];
	if (!Platform_CardRead(0, head, sizeof(head)))
	{
		problem = "CARD NOT READ";
		return false;
	}
	//"CARD", then the version this reader knows
	if ((head[0] != 'C') || (head[1] != 'A') || (head[2] != 'R') || (head[3] != 'D') || (head[4] != 1))
	{
		problem = "WRONG CARD FILE";
		return false;
	}
	//The stamp is every skin and picture name hashed, so a card packed for another build of the
	//game is refused here rather than drawn as rubbish
	if (Read32(head + 8) != CARD_STAMP)
	{
		problem = "WRONG CARD DATA";
		return false;
	}

	//the art section, found by name so that another section put beside it later changes nothing
	const uint8_t count = head[5];
	uint32_t length = 0;
	section = 0;
	for (uint8_t i = 0; i < count; i++)
	{
		uint8_t entry[CARD_SECTION];
		if (!Platform_CardRead(CARD_HEAD + (uint32_t)CARD_SECTION * i, entry, sizeof(entry)))
		{
			problem = "CARD NOT READ";
			return false;
		}
		if (memcmp(entry, CARD_SEC_IMAGES, 4) == 0)
		{
			section = Read32(entry + 4);
			length = Read32(entry + 8);
			break;
		}
	}
	if (!section)
	{
		problem = "NO ART ON CARD";
		return false;
	}

	//the section's own head says what it holds, which has to be what this build expects
	uint8_t sec[4];
	if (!Platform_CardRead(section, sec, sizeof(sec)))
	{
		problem = "CARD NOT READ";
		return false;
	}
	if ((sec[0] != CARD_SKIN_COUNT) || (sec[1] != CARD_IMAGE_COUNT))
	{
		problem = "WRONG CARD DATA";
		return false;
	}
	(void)length;

	ready = true;
	return CardImages_UseSkin(skin);
}

bool CardImages_UseSkin(uint8_t which)
{
	if (!ready || (which >= CARD_SKIN_COUNT))
		return false;
	//the descriptors are read once here, so drawing never walks the index
	const uint32_t base = section + 4 + (uint32_t)CARD_ENTRY * which * CARD_IMAGE_COUNT;
	for (uint8_t i = 0; i < CARD_IMAGE_COUNT; i++)
	{
		uint8_t entry[CARD_ENTRY];
		if (!Platform_CardRead(base + (uint32_t)CARD_ENTRY * i, entry, sizeof(entry)))
		{
			problem = "CARD NOT READ";
			ready = false;
			return false;
		}
		if ((entry[8] != CARD_FMT_RGB565)
#if CARD_HAS_ROWS
		    && (entry[8] != CARD_FMT_ROWS)
#endif
		   )
		{
			problem = "WRONG CARD DATA";
			ready = false;
			return false;
		}
		//the entry's offset is counted from its section, see tools/mkcard.py
		images[i].at = section + Read32(entry);
		images[i].w = Read16(entry + 4);
		images[i].h = Read16(entry + 6);
		images[i].fmt = entry[8];
		images[i].slot = -1;
	}
	skin = which;
	//what was cached belonged to the skin before this one
	arenaUsed = 0;
	return true;
}

//Empties the arena, for a screen that is about to draw something else entirely. Without this
//the pictures of the screen before it would sit here and the new one's would never fit
void CardImages_Reset(void)
{
	for (uint8_t i = 0; i < CARD_IMAGE_COUNT; i++)
		images[i].slot = -1;
	arenaUsed = 0;
}

const uint8_t* CardImages_Get(uint8_t image)
{
	if (!ready || (image >= CARD_IMAGE_COUNT))
		return NULL;
	return (const uint8_t*)&images[image];
}

uint16_t CardImages_Width(const uint8_t* image)
{
	return image ? ((const CardEntry*)image)->w : 0;
}

uint16_t CardImages_Height(const uint8_t* image)
{
	return image ? ((const CardEntry*)image)->h : 0;
}

//Puts the whole picture in the arena and answers where, or -1 when it does not belong there.
//Emptying the arena drops every picture's slot, so the slots are held in the descriptors and not
//in a table of their own: there is nothing to walk
static int16_t Cache(CardEntry* img)
{
#if CARDARENA == 0
	(void)img;
	return -1;
#else
	const uint32_t size = EntryBytes(img);
	if (size > CARD_CACHE_MAX)
		return -1;
	if (img->slot >= 0)
		return img->slot;
	//What is already in here stays. Emptying the arena to make room for one more picture threw
	//out whatever the screen draws most: znax's block sheet is 4480 bytes and its GO is 2812, so
	//in an arena of 5120 the first GO evicted the sheet, the 169 blocks of that same pass were
	//read off the card a row at a time, and the next frame's blocks evicted GO again.
	//A picture that does not fit in what is left is read instead, which costs a read each time it
	//is drawn: that is the right way round, since what fills the arena first is what the screen
	//was drawing first and most. CardImages_Reset empties it when the screen changes
	if (arenaUsed + size > CARDARENA)
		return -1;
	if (!Platform_CardRead(img->at, arena + arenaUsed, size))
		return -1;
	reads++;
	img->slot = (int16_t)arenaUsed;
	//the next picture starts on a four byte boundary, see the arena above
	arenaUsed = (uint16_t)((arenaUsed + size + 3) & ~3u);
	return img->slot;
#endif
}

const uint8_t* CardImages_Cached(const uint8_t* image)
{
#if CARDARENA > 0
	if (!image)
		return NULL;
#if CARD_HAS_ROWS
	//A picture kept as one colour a row holds no pixels to copy, see CARD_FMT_ROWS. The caller
	//would read it as though it did, so it is told there is nothing and asks for rows instead
	if (((const CardEntry*)image)->fmt != CARD_FMT_RGB565)
		return NULL;
#endif
	const int16_t slot = Cache((CardEntry*)image);
	return (slot >= 0) ? (arena + slot) : NULL;
#else
	(void)image;
	return NULL;
#endif
}

bool CardImages_Rows(const uint8_t* image, int x, int y, int count, int rows, uint16_t* dst)
{
	if (!image || !dst || (count <= 0) || (rows <= 0))
		return false;
	CardEntry* img = (CardEntry*)image;
	//Only when the rows lie together: a row of a picture is followed by the one under it, so a
	//run of whole rows is one run of bytes. A part of a row is not, and goes a row at a time
	if ((x != 0) || (count != (int)img->w))
	{
		for (int r = 0; r < rows; r++)
			if (!CardImages_Row(image, x, y + r, count, dst + (size_t)r * count))
				return false;
		return true;
	}
	if ((y < 0) || (y + rows > (int)img->h))
		return false;
#if CARD_HAS_ROWS
	if (img->fmt == CARD_FMT_ROWS)
	{
		//one colour a row, so the strip is filled and the card is not touched at all once the
		//handful of bytes it does hold have been read once
		for (int r = 0; r < rows; r++)
			if (!CardImages_Row(image, x, y + r, count, dst + (size_t)r * count))
				return false;
		return true;
	}
#endif
#if CARDARENA > 0
	const int16_t slot = Cache(img);
	if (slot >= 0)
	{
		memcpy(dst, arena + slot + (uint32_t)y * img->w * 2, (size_t)count * rows * 2);
		return true;
	}
#endif
	reads++;
	return Platform_CardRead(img->at + (uint32_t)y * img->w * 2, dst,
	                         (uint32_t)count * rows * 2);
}

bool CardImages_Row(const uint8_t* image, int x, int y, int count, uint16_t* dst)
{
	if (!image || !dst || (count <= 0))
		return false;
	CardEntry* img = (CardEntry*)image;
	if ((x < 0) || (y < 0) || (y >= (int)img->h) || (x + count > (int)img->w))
		return false;

#if CARD_HAS_ROWS
	if (img->fmt == CARD_FMT_ROWS)
	{
		//the whole row is one colour, see CARD_FMT_ROWS. The colours are two bytes a row and
		//fit the arena with room to spare, so after the first draw this costs no card read
		uint16_t colour;
#if CARDARENA > 0
		const int16_t at = Cache(img);
		if (at >= 0)
			memcpy(&colour, arena + at + (uint32_t)y * 2, sizeof(colour));
		else
#endif
		{
			reads++;
			if (!Platform_CardRead(img->at + (uint32_t)y * 2, &colour, sizeof(colour)))
				return false;
		}
		for (int c = 0; c < count; c++)
			dst[c] = colour;
		return true;
	}
#endif

#if CARDARENA > 0
	const int16_t slot = Cache(img);
	if (slot >= 0)
	{
		//from the arena, which is where a picture drawn more than once ends up
		memcpy(dst, arena + slot + ((uint32_t)y * img->w + x) * 2, (size_t)count * 2);
		return true;
	}
#endif
	//too big to keep, so a row at a time. A full screen background is 32 KB and is drawn as
	//eight row strips by the band renderer, which is one read of 2 KB a strip
	reads++;
	return Platform_CardRead(img->at + ((uint32_t)y * img->w + x) * 2, dst, (uint32_t)count * 2);
}

#endif
