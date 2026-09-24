#ifndef DEFINES_H
#define DEFINES_H

//the device comes first: the display library and SCREENBUFFER are device settings, see
//PlatformESPboy.h / PlatformSDL.h
#include "PlatformDevice.h"
#include <stdint.h>

//the ESPboy display
#define WINDOW_WIDTH 128
#define WINDOW_HEIGHT 128

//gamestates
#define GSQuit 0
#define GSIntro 1
#define GSGame 2
#define GSTitleScreen 3
#define GSTimeOver 4
#define GSReadyGo 5
#define GSCredits 6
#define GSGameTypeMenu 7
#define GSShowHighScores 8
#define GSGetHighScoreName 9

#define GSIntroInit 11
#define GSGameInit 12
#define GSTitleScreenInit 13
#define GSTimeOverInit 14
#define GSReadyGoInit 15
#define GSCreditsInit 16
#define GSGameTypeMenuInit 17
#define GSShowHighScoresInit 18
#define GSGetHighScoreNameInit 19

//gametypes
#define Fixed 0
#define Relative 1

#define NrOfRows 13
#define NrOfCols 13
#define NrOfBlockColors 5
#define BlockBlue 0
#define BlockYellow 1
#define BlockOrange 2
#define BlockGreen 3
#define BlockRed 4
#define MaxMusicFiles 26
#define TileWidth 8
#define TileHeight 8

//where the block at playfield x,y is drawn
#define BlockScreenX(x) ((x) * TileWidth + 7 + (x))
#define BlockScreenY(y) ((y) * TileHeight + 19)

#define SCALE 128.0f/240.0f

//the colour the images use for transparent pixels, RGB565 of (0,11,255)
#define COLOR_TRANSPARENT 0x005F

//the sizes of the images, the same in every skin (helperfuncs.cpp checks them)
#define fullScreenWidth 128
#define fullScreenHeight 128
#define blocksWidth 56
#define blocksHeight 40
#define cursorWidth 8
#define cursorHeight 8
#define creditsWidth 95
#define creditsHeight 58
#define menuWordWidth 74
#define menuWordHeight 19
#define timerWordWidth 113
#define timerWordHeight 20
#define selectGameWidth 111
#define selectGameHeight 20
#define goWidth 38
#define goHeight 37
#define readyWidth 88
#define readyHeight 43
#define timeOverWidth 113
#define timeOverHeight 39

#define skinDefault 0
#define skinBlackWhite 1

//FORCESKIN: -1 = the default skin, or the black & white one with a 1 bpp buffer, n = skin n
//(0 default, 1 black & white). There is no skin option in the game, so only the skin used is
//built in. A 1 bpp buffer has only two colours to show, so the
//black & white skin is the one it takes on its own. A build can still ask it for another one,
//whose shades then go through the brightness rule in SetBufferBit, and with DITHERING come out
//as a pattern of the two colours rather than as the nearer of them.
//Set by the device header or the build
#if !defined(FORCESKIN) || (FORCESKIN < 0)
  #undef FORCESKIN
  #if SCREENBUFFER == 1
  #define FORCESKIN skinBlackWhite
  #else
  #define FORCESKIN skinDefault
  #endif
#endif

#define FRAMERATE 30
//1 = every frame waits until 1/FRAMERATE of a second has passed, 0 = a frame starts as soon
//as the last one is done, to see how fast the game can go. The block animation and the music
//count frames, so without the lock they run faster as well. A build can set it itself
#ifndef FPSLOCK
#define FPSLOCK 1
#endif
//1 = the debug header (frame rate, free heap and stack) is always shown, Up + Down does not
//hide it. 0 = it starts hidden and Up + Down shows and hides it. A build can set it itself
#ifndef FORCEDEBUG
#define FORCEDEBUG 0
#endif
//1 = the colours of an image are spread over the ones the buffer can hold, so that a shade it
//has no colour for is a pattern of the two it does instead of the nearer of them. 0 = every
//colour becomes the nearest one there is, which shows as bands across anything that shades.
//An 8 bpp buffer is RGB332 and drops 2 bits of red, 3 of green and 3 of blue, and a 1 bpp buffer
//keeps only black and white, so both have something to spread. A 16 bpp buffer holds every colour
//of the image as it is and is left alone. A build can set this itself, see DitherSpread in
//Platform.h
#ifndef DITHERING
#define DITHERING 0
#endif

#define MAXLENHISCORENAME 8

struct SHighScore
{
    char PName[MAXLENHISCORENAME+1];
    int32_t PScore;
};
typedef struct SHighScore SHighScore;

struct SPoint
{
    int X,Y;
};

typedef struct SPoint SPoint;

//the ESPboy version saved this at the start of its EEPROM, it is kept there so its high scores
//are still read: a name, 3 bytes padding and a 32 bit score per high score
#pragma pack(push, 1)
struct SaveData {
    uint16_t magic;  // always first
    SHighScore HighScores[2][10];
    uint8_t crc;
};
#pragma pack(pop)
typedef struct SaveData SaveData;


#endif
