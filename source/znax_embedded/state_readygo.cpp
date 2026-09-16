
#include "gamefuncs.h"
#include "common.h"
#include "cworldparts.h"
#include "helperfuncs.h"
#include "sound.h"
#include "state_readygo.h"

void ReadyGoInit()
{
    srand(Platform_RandomSeed());
    CWorldParts_NewGame(World);
    Score = 0;
    Time = getMillis();
    Counter=0;
    if(GameType == Relative)
        Timer = 150;
    else
        Timer = 300;
    needRedraw = 1;
}
void ReadyGo()
{

    if(GameState == GSReadyGoInit)
    {
        ReadyGoInit();
        GameState = GSReadyGo;
    }

    if ((currButtons & BUTTON_B) && !(prevButtons & BUTTON_B))
    {
        playMenuBackSound();
        GameState = GSTitleScreenInit;
    }

    const uint8_t* overlay = NULL;
    int overlayWidth = 0, overlayHeight = 0;
    switch(Counter)
    {
        case 1 :
            overlay = imgReady;
            overlayWidth = readyWidth;
            overlayHeight = readyHeight;
            break;
        case 2 :
            overlay = imgGo;
            overlayWidth = goWidth;
            overlayHeight = goHeight;
            break;
        case 3 :
            GameState = GSGameInit;
            break;
    }
    // the game goes on on the same screen, without the overlay
    if ((GameState == GSReadyGo) || (GameState == GSGameInit))
        DrawGameScreen(false, overlay, overlayWidth, overlayHeight);
    if (Time < getMillis())
    {
        Counter++;
        if(Counter == 1)
        {
            SelectMusic(musNone, 0);
            SelectMusic(musReady, 0);
        }
        if (Counter == 2)
        {
            SelectMusic(musNone, 0);
            SelectMusic(musGo, 0);
            Time = getMillis() + 400;
        }
        else
            Time = getMillis() + 900;
    }

}

