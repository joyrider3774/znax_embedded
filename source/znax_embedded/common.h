#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include "defines.h"
#include "Platform.h"
#include "cworldparts.h"
#include "cselector.h"
#include "cgametypemenu.h"
#include "cmainmenu.h"

extern bool GlobalSoundEnabled;
extern int Timer, AddToScore;
extern int GameState;
extern long int Score;
extern int GameType;
extern SaveData saveData;
extern CSelector* Selector;
extern CGameTypeMenu *MenuGameType;
extern uint32_t Time;
extern uint32_t ScoreTimer;
extern int Counter;
extern CMainMenu *MainMenu;
extern uint8_t prevButtons, currButtons;
extern bool debugMode;
//set when a screen has to be drawn again as a whole, the screens only draw what changed
extern uint8_t needRedraw;
extern int movesLeft;

//the colours of the skin, set by preloadImages
extern uint16_t ColorStatusText, ColorScoreText, ColorScoreTextNew;

//the images of the skin, set by preloadImages. The blocks and the cursor are raw RGB565, the
//others run length encoded
extern const uint8_t* imgBackground, *imgHighScores, *imgIntro1, *imgIntro2, *imgTitleScreen;
extern const uint8_t* imgCredits, *imgCredits1, *imgCredits2, *imgFixedTimer1, *imgFixedTimer2, *imgGo,
	*imgHighScores1, *imgHighScores2, *imgPlay1, *imgPlay2, *imgReady, *imgRelativeTimer1, *imgRelativeTimer2,
	*imgSelectGame, *imgTimeOver;
extern const uint8_t* imgBlocks, *imgCursor;
#endif
