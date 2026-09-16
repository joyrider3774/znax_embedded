#include "common.h"
#include "cmainmenu.h"
#include "helperfuncs.h"
#include "sound.h"

//where the menu words are
#define wordX ((WINDOW_WIDTH-menuWordWidth) >> 1)
#define playY ((int)(83*SCALE))
#define highScoresY ((int)(123*SCALE))
#define creditsY ((int)(163*SCALE))

//the selection the menu words on screen show, -1 when they have to be drawn again
static int shownSelection = -1;

// constructor of main menu will Load the graphics and set the current selection to 1 (io newgame)
CMainMenu* CMainMenu_Create()
{
    CMainMenu* Result = (CMainMenu*) malloc(sizeof(CMainMenu));
    Result->Selection = 1;
    return Result;
}

// destructor
void CMainMenu_Destroy(CMainMenu* MainMenu)
{
    free(MainMenu);
    MainMenu = NULL;
}

// return the current selection
int CMainMenu_GetSelection(CMainMenu* MainMenu)
{
    return MainMenu->Selection;
}

// Increase the selection if it goes to far set i to the first selection
void CMainMenu_NextItem(CMainMenu* MainMenu)
{
    MainMenu->Selection++;
    if (MainMenu->Selection == 4)
        MainMenu->Selection = 1;
    playMenuSelectSound();
}

// decrease the selection if it goes to low set it to the last selection
void CMainMenu_PreviousItem(CMainMenu* MainMenu)
{
    MainMenu->Selection--;
    if (MainMenu->Selection == 0)
        MainMenu->Selection = 3;
    playMenuSelectSound();
}

// the title screen under a menu word, the words are transparent
static void DrawWordBackground(int y)
{
    drawImageRLEPart(wordX, y, wordX, y, menuWordWidth, menuWordHeight, imgTitleScreen, fullScreenWidth, fullScreenHeight, false);
}

// Draw the main menu, only what changed since the last time
void CMainMenu_Draw(CMainMenu* MainMenu)
{
    if (needRedraw)
    {
        needRedraw = 0;
        drawImageRLE(0, 0, fullScreenWidth, fullScreenHeight, imgTitleScreen);
        shownSelection = -1;
    }
    else if (MainMenu->Selection != shownSelection)
    {
        DrawWordBackground(playY);
        DrawWordBackground(highScoresY);
        DrawWordBackground(creditsY);
    }

    if (MainMenu->Selection == shownSelection)
        return;
    shownSelection = MainMenu->Selection;

    if (MainMenu->Selection == 1)
    {
        drawImageRLETransparent(wordX, playY, menuWordWidth, menuWordHeight, imgPlay2);
    }
    else
    {
        drawImageRLETransparent(wordX, playY, menuWordWidth, menuWordHeight, imgPlay1);
    }

    if (MainMenu->Selection == 2)
    {
        drawImageRLETransparent(wordX, highScoresY, menuWordWidth, menuWordHeight, imgHighScores2);
    }
    else
    {
        drawImageRLETransparent(wordX, highScoresY, menuWordWidth, menuWordHeight, imgHighScores1);
    }


    if (MainMenu->Selection == 3)
    {
        drawImageRLETransparent(wordX, creditsY, menuWordWidth, menuWordHeight, imgCredits2);
    }
    else
    {
        drawImageRLETransparent(wordX, creditsY, menuWordWidth, menuWordHeight, imgCredits1);
    }
}
