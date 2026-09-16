
#include "gamefuncs.h"
#include "common.h"
#include "cworldparts.h"
#include "cselector.h"
#include "helperfuncs.h"
#include "state_game.h"
#include "sound.h"

void GameInit()
{
    AddToScore = 0;
    if(GameType == Relative)
        Timer = 150;
    else
        Timer = 300;
    Time = getMillis();
    ScoreTimer = 0;
    Selector->CurrentPoint.X = NrOfCols / 2;
    Selector->CurrentPoint.Y = NrOfRows / 2;
    // SelectMusic(musNone, 0);
    // SelectMusic(musStart, 0);
}

void Game()
{
    if (GameState == GSGameInit)
    {
        GameInit();
        GameState = GSGame;
    }

    if ((currButtons & BUTTON_LEFT) && !(prevButtons & BUTTON_LEFT))
        CSelector_SetPosition(Selector, Selector->CurrentPoint.X -1,Selector->CurrentPoint.Y);
    if ((currButtons & BUTTON_RIGHT) && !(prevButtons & BUTTON_RIGHT))
        CSelector_SetPosition(Selector, Selector->CurrentPoint.X +1,Selector->CurrentPoint.Y);
    if ((currButtons & BUTTON_UP) && !(prevButtons & BUTTON_UP))
        CSelector_SetPosition(Selector, Selector->CurrentPoint.X,Selector->CurrentPoint.Y-1);
    if ((currButtons & BUTTON_DOWN) && !(prevButtons & BUTTON_DOWN))
        CSelector_SetPosition(Selector, Selector->CurrentPoint.X,Selector->CurrentPoint.Y+1);
    if ((currButtons & BUTTON_B) && !(prevButtons & BUTTON_B))
    {
        playMenuBackSound();
        GameState = GSTitleScreenInit;
    }
    if ((currButtons & BUTTON_A) && !(prevButtons & BUTTON_A))
    {
        AddToScore+= CWorldParts_Select(World, Selector->CurrentPoint.X,Selector->CurrentPoint.Y);
        if(AddToScore != 0)
        {
            ScoreTimer = getMillis() + 700;
        }
    }
    if(AddToScore !=0)
    {
        if(ScoreTimer <= getMillis())
        {
            ScoreTimer = 0;
            Score +=AddToScore;
            if (GameType == Relative)
                Timer += AddToScore / 400;
            AddToScore = 0;
        }
    }
    if (Time +1000 < getMillis())
    {
        Timer-= 1;
        switch (Timer)
        {
            case 60*3 :
                SelectMusic(musNone,0);
                SelectMusic(mus5Min,0);
                break;
            case 60:
                SelectMusic(musNone,0);
                SelectMusic(mus3Min,0);
                break;
            case 3 :
                playThreeSound();
                break;
            case 2 :
                playTwoSound();
                break;
            case 1 :
                playOneSound();
                break;
            case 0 :
                if(ScoreTimer != 0)
                {
                    Score += AddToScore;
                    if(GameType == Relative)
                        Timer += AddToScore / 200;
                }
                if(Timer == 0)
                {
                    //if (GlobalSoundEnabled)
                    //    Mix_PlayChannel(-1,Sounds[SND_TIMEOVER],0);
                    GameState = GSTimeOverInit;
                }
                break;
        }
        Time = getMillis();
    }
    // the board, cursor and status bar, the time over screen draws over the same screen
    if ((GameState == GSGame) || (GameState == GSTimeOverInit))
        DrawGameScreen(true, NULL, 0, 0);
}
