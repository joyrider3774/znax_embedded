
#include <string.h>
#include "gamefuncs.h"
#include "common.h"
#include "helperfuncs.h"
#include "sound.h"
#include "state_showhighscores.h"

int ScoreType;
//the scores on screen, -1 when the screen has to be drawn again
static int shownScoreType = -1;

void ShowHighScoresInit()
{
    ScoreType = GameType;
    needRedraw = 1;
}

void ShowHighScores()
{
    if (GameState == GSShowHighScoresInit)
    {
        ShowHighScoresInit();
        GameState = GSShowHighScores;
    }

    int Teller = 0;
    char Text[32];
    if ((currButtons & BUTTON_A) && !(prevButtons & BUTTON_A))
    {

        if (ScoreType == Fixed)
        {
            playMenuAcknowlege();
            ScoreType = Relative;
        }
        else
        {
            playMenuBackSound();
            GameState = GSTitleScreenInit;
        }
    }
    if ((currButtons & BUTTON_B) && !(prevButtons & BUTTON_B))
    {
        playMenuBackSound();
        GameState = GSTitleScreenInit;
    }

    // nothing to draw when the screen was left, or when it already shows these scores
    if ((GameState != GSShowHighScores) || (!needRedraw && (ScoreType == shownScoreType)))
        return;
    needRedraw = 0;
    shownScoreType = ScoreType;

    drawImageRLE(0, 0, fullScreenWidth, fullScreenHeight, imgHighScores);
    switch(ScoreType)
    {
        case Fixed :
            snprintf(Text,sizeof(Text),"Fixed Timer" );
            break;
        case Relative :
            snprintf(Text,sizeof(Text),"Relative Timer" );
            break;
    }

    printText((WINDOW_WIDTH - (strlen(Text)*6)) >> 1,(int)(226*SCALE),Text,ColorScoreText,ColorScoreText,1);

    for(Teller = 0;Teller<10;Teller++)
    {
        snprintf(Text,sizeof(Text),"%2d.%s",Teller+1,saveData.HighScores[ScoreType][Teller].PName);
	    printText((int)(3*SCALE),(int)((62+Teller*16)*SCALE),Text,ColorScoreText,ColorScoreText,1);
	    snprintf(Text,sizeof(Text),"%7ld",(long)saveData.HighScores[ScoreType][Teller].PScore);
	    printText((int)(155*SCALE),(int)((62+Teller*16)*SCALE),Text,ColorScoreText,ColorScoreText,1);
    }
}
