#include <stdio.h>
#include <stdlib.h>
#include "defines.h"
#include "common.h"
#include "cselector.h"
#include "cgametypemenu.h"
#include "cmainmenu.h"

struct CWorldParts;

bool GlobalSoundEnabled = true;
int Timer = 150, AddToScore = 0;
int GameState = GSIntroInit;

long int Score;
int GameType = Fixed;
SaveData saveData;
CSelector* Selector;
CGameTypeMenu *MenuGameType;
uint32_t Time;
uint32_t ScoreTimer;
int Counter;
CMainMenu *MainMenu;
uint8_t prevButtons, currButtons;
bool debugMode = false;
uint8_t needRedraw = 1;
int movesLeft = 0;

uint16_t ColorStatusText, ColorScoreText, ColorScoreTextNew;

const uint8_t* imgBackground, *imgHighScores, *imgIntro1, *imgIntro2, *imgTitleScreen;
const uint8_t* imgCredits, *imgCredits1, *imgCredits2, *imgFixedTimer1, *imgFixedTimer2, *imgGo,
	*imgHighScores1, *imgHighScores2, *imgPlay1, *imgPlay2, *imgReady, *imgRelativeTimer1, *imgRelativeTimer2,
	*imgSelectGame, *imgTimeOver;
const uint8_t* imgBlocks, *imgCursor;
