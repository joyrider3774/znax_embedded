
#include "defines.h"
#include "gamefuncs.h"
#include "common.h"
#include "helperfuncs.h"
#include "state_credits.h"
#include "sound.h"

void CreditsInit()
{
    needRedraw = 1;
}

void Credits()
{
    if (GameState == GSCreditsInit)
    {
        CreditsInit();
        GameState = GSCredits;
    }

    if(currButtons && !prevButtons)
    {
        GameState = GSTitleScreenInit;
        playMenuBackSound();
    }

    if ((GameState == GSCredits) && needRedraw)
    {
        needRedraw = 0;
        drawImageRLE(0, 0, fullScreenWidth, fullScreenHeight, imgTitleScreen);
        drawImageRLETransparent((int)(33*SCALE), (int)(85*SCALE), creditsWidth, creditsHeight, imgCredits);
    }
}
