#include <stdio.h>
#include <stdlib.h>
#include "common.h"
#include "defines.h"
#include "cgametypemenu.h"
#include "helperfuncs.h"
#include "sound.h"

//where the words are
#define selectGameX ((WINDOW_WIDTH-selectGameWidth) >> 1)
#define selectGameY ((int)(77*SCALE))
#define timerX ((WINDOW_WIDTH-timerWordWidth) >> 1)
#define fixedTimerY ((int)(114*SCALE))
#define relativeTimerY ((int)(151*SCALE))

//the selection the words on screen show, -1 when they have to be drawn again
static int shownSelection = -1;

// constructor of main menu will Load the graphics and set the current selection to 1 (io newgame)
CGameTypeMenu* CGameTypeMenu_Create()
{
    CGameTypeMenu* Result = (CGameTypeMenu*) malloc(sizeof(CGameTypeMenu));
    Result->Selection = Fixed;
    return Result;
}

// return the current selection
int CGameTypeMenu_GetSelection(CGameTypeMenu* GameTypeMenu)
{
    return GameTypeMenu->Selection;
}

// Destructor will free the surface images
void CGameTypeMenu_Destroy(CGameTypeMenu* GameTypeMenu)
{
    free(GameTypeMenu);
    GameTypeMenu = NULL;
}

// Increase the selection if it goes to far set i to the first selection
void CGameTypeMenu_NextItem(CGameTypeMenu* GameTypeMenu)
{
    GameTypeMenu->Selection++;
    if (GameTypeMenu->Selection == 2)
        GameTypeMenu->Selection = 0;
    playMenuSelectSound();
}

// decrease the selection if it goes to low set it to the last selection
void CGameTypeMenu_PreviousItem(CGameTypeMenu* GameTypeMenu)
{
    GameTypeMenu->Selection--;
    if (GameTypeMenu->Selection == -1)
        GameTypeMenu->Selection = 1;
    playMenuSelectSound();
}

// the title screen under a timer word, the words are transparent
static void DrawWordBackground(int y)
{
    drawImageRLEPart(timerX, y, timerX, y, timerWordWidth, timerWordHeight, imgTitleScreen, fullScreenWidth, fullScreenHeight, false);
}

// Draw the main menu, only what changed since the last time
void CGameTypeMenu_Draw(CGameTypeMenu* GameTypeMenu)
{
    if (needRedraw)
    {
        needRedraw = 0;
        drawImageRLE(0, 0, fullScreenWidth, fullScreenHeight, imgTitleScreen);
        drawImageRLETransparent(selectGameX, selectGameY, selectGameWidth, selectGameHeight, imgSelectGame);
        shownSelection = -1;
    }
    else if (GameTypeMenu->Selection != shownSelection)
    {
        DrawWordBackground(fixedTimerY);
        DrawWordBackground(relativeTimerY);
    }

    if (GameTypeMenu->Selection == shownSelection)
        return;
    shownSelection = GameTypeMenu->Selection;

    if (GameTypeMenu->Selection == Fixed)
    {
        drawImageRLETransparent(timerX, fixedTimerY, timerWordWidth, timerWordHeight, imgFixedTimer2);
    }
    else
    {
        drawImageRLETransparent(timerX, fixedTimerY, timerWordWidth, timerWordHeight, imgFixedTimer1);
    }

    if (GameTypeMenu->Selection == Relative)
    {
        drawImageRLETransparent(timerX, relativeTimerY, timerWordWidth, timerWordHeight, imgRelativeTimer2);
    }
    else
    {
        drawImageRLETransparent(timerX, relativeTimerY, timerWordWidth, timerWordHeight, imgRelativeTimer1);
    }
}
