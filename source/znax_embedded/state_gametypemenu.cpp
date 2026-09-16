
#include "cgametypemenu.h"
#include "gamefuncs.h"
#include "common.h"
#include "sound.h"
#include "state_gametypemenu.h"

void GameTypeMenuInit()
{
    GameType = Fixed;
    needRedraw = 1;
}
void GameTypeMenu()
{
    if (GameState == GSGameTypeMenuInit)
    {
        GameTypeMenuInit();
        GameState = GSGameTypeMenu;
    }
    if ((currButtons & BUTTON_UP) && !(prevButtons & BUTTON_UP))
        CGameTypeMenu_NextItem(MenuGameType);
    if ((currButtons & BUTTON_DOWN) && !(prevButtons & BUTTON_DOWN))
        CGameTypeMenu_PreviousItem(MenuGameType);
    if ((currButtons & BUTTON_A) && !(prevButtons & BUTTON_A))
    {
        playMenuAcknowlege();
        switch(CGameTypeMenu_GetSelection(MenuGameType))
        {
            case Fixed:
                GameType = Fixed;
                break;
            case Relative:
                GameType = Relative;
                break;
        }
        GameState = GSReadyGoInit;
    }
    if ((currButtons & BUTTON_B) && !(prevButtons & BUTTON_B))
    {
        playMenuBackSound();
        GameState = GSTitleScreenInit;
    }
    // nothing to draw when the menu was left
    if (GameState == GSGameTypeMenu)
        CGameTypeMenu_Draw(MenuGameType);
}

