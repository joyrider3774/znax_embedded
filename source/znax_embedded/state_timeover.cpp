
#include "gamefuncs.h"
#include "common.h"
#include "cworldparts.h"
#include "helperfuncs.h"
#include "sound.h"
#include "state_timeover.h"
#include "state_gethighscorename.h"

void TimeOverInit()
{
    Time = getMillis() + 1250;
    Counter=0;
    Timer = 0;
    CWorldParts_DeSelect(World, false);
}

void TimeOver()
{
    if (GameState == GSTimeOverInit)
    {
        TimeOverInit();
        GameState = GSTimeOver;
    }
    if ((currButtons & BUTTON_B) && !(prevButtons & BUTTON_B))
    {
        playMenuBackSound();
        GameState = GSTitleScreenInit;
    }

    const uint8_t* overlay = NULL;
    switch(Counter)
    {
        case 0 :
            overlay = imgTimeOver;
            break;
    }
    if (GameState == GSTimeOver)
        DrawGameScreen(false, overlay, timeOverWidth, timeOverHeight);
    if (Time < getMillis())
    {
        GameState = GSGetHighScoreNameInit;
    }
}

