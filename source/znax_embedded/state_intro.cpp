
#include "gamefuncs.h"
#include "common.h"
#include "helperfuncs.h"
#include "state_intro.h"

int IntroScreenNr;
//the intro screen on screen, 0 when it has to be drawn again
static int shownIntroScreenNr = 0;

void IntroInit()
{
    IntroScreenNr = 1;
    Time = getMillis();
    needRedraw = 1;
}

void Intro()
{
    if(GameState == GSIntroInit)
    {
        IntroInit();
        GameState = GSIntro;
    }
    if(currButtons)
    {
        GameState = GSTitleScreenInit;
    }

    // only when another intro screen is shown
    if ((GameState == GSIntro) && (needRedraw || (IntroScreenNr != shownIntroScreenNr)))
    {
        needRedraw = 0;
        shownIntroScreenNr = IntroScreenNr;
        switch(IntroScreenNr)
        {
            case 1 :
                drawImageRLE(0, 0, fullScreenWidth, fullScreenHeight, imgIntro1);
                break;
            case 2 :
                drawImageRLE(0, 0, fullScreenWidth, fullScreenHeight, imgIntro2);
                break;
        }
    }

    if(Time + 3700 < getMillis())
    {
        IntroScreenNr++;
        if(IntroScreenNr > 2)
            GameState = GSTitleScreenInit;
        Time = getMillis();
    }

}
