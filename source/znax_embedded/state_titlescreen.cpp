
#include "gamefuncs.h"
#include "common.h"
#include "cmainmenu.h"
#include "state_titlescreen.h"
#include "sound.h"

void TitleScreenInit()
{
    needRedraw = 1;
}

void TitleScreen()
{
    if (GameState == GSTitleScreenInit)
    {
        TitleScreenInit();
        GameState = GSTitleScreen;
    }

    if ((currButtons & BUTTON_UP) && !(prevButtons & BUTTON_UP))
        CMainMenu_PreviousItem(MainMenu);
    if ((currButtons & BUTTON_DOWN) && !(prevButtons & BUTTON_DOWN))
        CMainMenu_NextItem(MainMenu);
    if ((currButtons & BUTTON_A) && !(prevButtons & BUTTON_A))
    {
        playMenuAcknowlege();
        switch(CMainMenu_GetSelection(MainMenu))
        {
            case 1:
                GameState = GSGameTypeMenuInit;
                break;
            case 2:
                //so that it shows both gameplay modes
                GameType = Fixed;
                GameState = GSShowHighScoresInit;
                break;
            case 3:
                GameState = GSCreditsInit;
                break;
            default:
                break;
        }
    }
    // nothing to draw when the title screen was left
    if (GameState == GSTitleScreen)
        CMainMenu_Draw(MainMenu);
}
