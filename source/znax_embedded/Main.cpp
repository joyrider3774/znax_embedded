#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <float.h>
#include <math.h>
#include <string.h>
#include "defines.h"
#include "common.h"
#include "cgametypemenu.h"
#include "cmainmenu.h"
#include "cselector.h"
#include "cworldparts.h"
#include "gamefuncs.h"
#include "helperfuncs.h"
#include "state_gametypemenu.h"
#include "state_credits.h"
#include "state_titlescreen.h"
#include "state_timeover.h"
#include "state_readygo.h"
#include "state_intro.h"
#include "state_game.h"
#include "state_showhighscores.h"
#include "state_gethighscorename.h"
#include "sound.h"

//The program itself, Game_Setup and Game_Loop are called by the device's own source

const uint32_t timePerFrame =  1000000 / FRAMERATE;
static float frameRate = 0;
static uint32_t currentTime = 0, lastTime = 0, frameTime = 0;
static bool endFrame = true;

static uint32_t getFreeRam() {
  return Platform_FreeHeap();
}

static uint32_t getFreeStack() {
	return Platform_FreeStack();
}

//lowest free heap seen since boot, sampled at the end of Game_Setup and of every frame.
//Something allocated and freed again within one frame does not show up here
static uint32_t lowestFreeRam = UINT32_MAX;

static void trackLowestFreeRam()
{
    uint32_t freeRam = getFreeRam();
    if (freeRam < lowestFreeRam)
        lowestFreeRam = freeRam;
}

static void printDebugCpuRamLoad()
{
    if(debugMode || FORCEDEBUG)
    {
        //the text is only put together a few times a second: every frame it would cost the
        //formatting and the heap and stack readings for figures nobody can read that fast.
        //It is still drawn every frame, the game may have been drawn over it
        static char debuginfo[80] = "";
        static uint32_t lastUpdate = 0;
        uint32_t now = Platform_Micros();
        if ((debuginfo[0] == '\0') || (now - lastUpdate >= 250000))
        {
            lastUpdate = now;
            int fps_int = (int)frameRate;
            int fps_frac = (int)((frameRate - fps_int) * 100);
            //S is the least sketch stack that has been free since boot, out of 4096 bytes
            //L: is the lowest free heap since boot, in the same column as R: on the line above
            snprintf(debuginfo, sizeof(debuginfo), "F:%3d.%2d R:%3" PRIu32 " \nS:%4" PRIu32 "   L:%3" PRIu32 " ", fps_int, fps_frac, getFreeRam(), getFreeStack(), lowestFreeRam);
            //Platform_Log("%s\n", debuginfo);
        }
        printText(0, 0, debuginfo, SCREEN.color565(255,255,255), SCREEN.color565(0,0,0), 1);
    }
}

void Game_Setup(void)
{
    Platform_Init("Znax v1.0");
    debugMode = false;
    needRedraw = 1;
    LoadHighScores();
    initSound();
    initMusic();
    setSoundOn(true);
    setMusicOn(true);
    preloadImages();
    //with a 1 bpp buffer, the colours its set and clear bits are shown in. The skin is
    //always black & white there
    Platform_SetBufferColors(SCREEN.color565(255,255,255), SCREEN.color565(0,0,0));
    World = CWorldParts_Create();
    Selector = CSelector_Create(NrOfCols / 2, NrOfRows / 2);
    MenuGameType = CGameTypeMenu_Create();
    MainMenu = CMainMenu_Create();
    trackLowestFreeRam();
    currentTime = Platform_Micros();
    lastTime = 0;
}

void Game_Loop(void)
{
    currentTime = Platform_Micros();
    frameTime  = currentTime - lastTime;
#if FPSLOCK
    if((frameTime < timePerFrame) || !endFrame)
       return;
#else
    //no lock, a frame starts as soon as the last one is done
    if(!endFrame)
       return;
#endif
    endFrame = false;
    //without the lock two frames can start within the same microsecond on a fast PC
    frameRate = 1000000.0 / (frameTime ? frameTime : 1);
    lastTime = currentTime;
    //keeps the milliseconds counting even on screens that do not ask for them
    getMillis();
    musicTimer();
    prevButtons = currButtons;
    currButtons = Platform_GetButtons();

    if((currButtons & BUTTON_UP) && (currButtons & BUTTON_DOWN) && !(prevButtons & BUTTON_DOWN))
    {
        debugMode = !debugMode;
        //the screens only draw what changed, the debug header has to be drawn over
        needRedraw = 1;
    }

    switch(GameState)
    {
        case GSGame :
        case GSGameInit:
            Game();
            break;
        case GSTitleScreen:
        case GSTitleScreenInit:
            TitleScreen();
            break;
        case GSIntro :
        case GSIntroInit :
            Intro();
            break;
        case GSReadyGo:
        case GSReadyGoInit:
            ReadyGo();
            break;
        case GSTimeOver:
        case GSTimeOverInit:
            TimeOver();
            break;
        case GSCredits:
        case GSCreditsInit:
            Credits();
            break;
        case GSGameTypeMenu:
        case GSGameTypeMenuInit:
            GameTypeMenu();
            break;
        case GSShowHighScores:
        case GSShowHighScoresInit:
            ShowHighScores();
            break;
        case GSGetHighScoreName:
        case GSGetHighScoreNameInit:
            GetHighScoreName();
            break;
        default :
            break;
    }

    trackLowestFreeRam();
    printDebugCpuRamLoad();
    Platform_PresentFrame();
    endFrame = true;
}
