
#include <string.h>
#include "gamefuncs.h"
#include "common.h"
#include "helperfuncs.h"
#include "state_gethighscorename.h"
#include "sound.h"

//one more than the name needs: the name can grow to MAXLENHISCORENAME + 1 characters and its end
//is written one further
char Name[MAXLENHISCORENAME+2];
bool NameEnd=false,NameSubmitChanges=false;
int NameMaxSelection=0, NameSelection = 0,asci=97;
int ScorePlace;
//the name on screen, the line of the new high score is drawn again when it changes
static char shownName[MAXLENHISCORENAME+2];

void GetHighScoreNameInit()
{
	ScorePlace = -1;
	for(int Teller1 =0;Teller1<10;Teller1++)
	{
		if(saveData.HighScores[GameType][Teller1].PScore < Score)
		{
			ScorePlace = Teller1;
			break;
		}
	}

	if (ScorePlace == -1)
	{
		GameState = GSShowHighScoresInit;
		return;
	}
	SelectMusic(musNone, 0);
    SelectMusic(musWinner, 0);
	NameEnd=false;
	NameSubmitChanges=false;
	NameMaxSelection=0;
	NameSelection = 0;
	asci=97;
	memset(Name, 0, sizeof(Name));
	NameMaxSelection = 0;
	Name[NameMaxSelection]=chr(asci);
	needRedraw = 1;
}

// the line of the new high score
static void DrawNewScoreLine()
{
	char Msg[32];
	snprintf(Msg,sizeof(Msg),"%2d.%s",ScorePlace+1,Name);
	printText((int)(3*SCALE),(int)((62+ScorePlace*16)*SCALE),Msg,ColorScoreTextNew,ColorScoreTextNew,1);
	snprintf(Msg,sizeof(Msg),"%7ld",(long)Score);
	printText((int)(155*SCALE),(int)((62+ScorePlace*16)*SCALE),Msg,ColorScoreTextNew,ColorScoreTextNew,1);
	strcpy(shownName, Name);
}

void GetHighScoreName()
{
	char NameIn[21];
	if(GameState == GSGetHighScoreNameInit)
	{
		GetHighScoreNameInit();
		if(GameState == GSShowHighScoresInit)
			return;
		GameState = GSGetHighScoreName;
	}
	char Tekst[100];
	if ((currButtons & BUTTON_LEFT) && !(prevButtons & BUTTON_LEFT))
	{
		if (NameSelection > 0)
		{	NameSelection--;
			asci = ord(Name[NameSelection]);
			playMenuBackSound();
		}
	}
	if ((currButtons & BUTTON_RIGHT) && !(prevButtons & BUTTON_RIGHT))
	{
		if (NameSelection < MAXLENHISCORENAME)
		{
			NameSelection++;
			if (NameSelection > NameMaxSelection)
			{
				Name[NameSelection] = chr(97);
				Name[NameSelection+1] = '\0';
				NameMaxSelection=NameSelection;
			}
			asci = ord(Name[NameSelection]);
			playMenuSelectSound();
		}
	}
	if ((currButtons & BUTTON_UP) && !(prevButtons & BUTTON_UP))
	{
		asci++;
		if (asci==123)
		{
			asci=32;
		}
		if (asci==33)
		{
			(asci=48);
		}
		if (asci==58)
		{
			asci=97;
		}
		Name[NameSelection] = chr(asci);
		playMenuSelectSound();
	}
	if ((currButtons & BUTTON_DOWN) && !(prevButtons & BUTTON_DOWN))
	{
		asci--;
		if(asci==96)
		{
			asci=57;
		}
		if(asci==47)
		{
			asci=32;
		}
		if(asci==31)
		{
			asci=122;
		}
		Name[NameSelection] = chr(asci);
		playMenuSelectSound();
	}
	if ((currButtons & BUTTON_A) && !(prevButtons & BUTTON_A))
	{
		playMenuAcknowlege();
		NameEnd = true;
		NameSubmitChanges=true;
	}
	if ((currButtons & BUTTON_B) && !(prevButtons & BUTTON_B))
	{
		playMenuBackSound();
		NameEnd=true;
		NameSubmitChanges=false;
	}

	// the whole screen when it is new, after that only the line of the name when the name changed
	if (needRedraw)
	{
		needRedraw = 0;
		drawImageRLE(0, 0, fullScreenWidth, fullScreenHeight, imgHighScores);
		char Msg[32];
		for(int Teller = 0;Teller<9;Teller++)
		{
			if(Teller < ScorePlace)
			{
				snprintf(Msg,sizeof(Msg),"%2d.%s",Teller+1,saveData.HighScores[GameType][Teller].PName);
				printText((int)(3*SCALE),(int)((62+Teller*16)*SCALE),Msg,ColorScoreText,ColorScoreText,1);
				snprintf(Msg,sizeof(Msg),"%7ld",(long)saveData.HighScores[GameType][Teller].PScore);
				printText((int)(155*SCALE),(int)((62+Teller*16)*SCALE),Msg,ColorScoreText,ColorScoreText,1);
			}
			else
			{
				snprintf(Msg,sizeof(Msg),"%2d.%s",Teller+2,saveData.HighScores[GameType][Teller].PName);
				printText((int)(3*SCALE),(int)((62+(Teller+1)*16)*SCALE),Msg,ColorScoreText,ColorScoreText,1);
				snprintf(Msg,sizeof(Msg),"%7ld",(long)saveData.HighScores[GameType][Teller].PScore);
				printText((int)(155*SCALE),(int)((62+(Teller+1)*16)*SCALE),Msg,ColorScoreText,ColorScoreText,1);
			}
		}
		DrawNewScoreLine();

		snprintf(Tekst,sizeof(Tekst),"^ v < > A=Ok B=Cancel" );
		printText((int)(3*SCALE),(int)(226*SCALE),Tekst,ColorScoreText,ColorScoreText,1);
	}
	else if (strcmp(Name, shownName) != 0)
	{
		// the high scores screen under the line, then the line again
		const int y = (int)((62+ScorePlace*16)*SCALE);
		drawImageRLEPart(0, y, 0, y, WINDOW_WIDTH, 8, imgHighScores, fullScreenWidth, fullScreenHeight, false);
		DrawNewScoreLine();
	}

	if(NameEnd)
	{
		GameState = GSShowHighScoresInit;
	}

	if(GameState != GSGetHighScoreName)
	{
		Name[NameMaxSelection+1] = '\0';
		while ((Name[0] == ' ') && (NameMaxSelection>-1))
		{
			for (int Teller=0;Teller<NameMaxSelection;Teller++)
				Name[Teller] = Name[Teller+1];
			NameMaxSelection--;
		}
		if (NameMaxSelection>-1)
			while ((Name[NameMaxSelection] == ' ') && (NameMaxSelection>0))
			{
				Name[NameMaxSelection] = '\0';
				NameMaxSelection--;
			}

		memset(NameIn, 0, (MAXLENHISCORENAME+1) * sizeof(char));
		if (!NameSubmitChanges)
		{
			snprintf(NameIn,sizeof(NameIn),"%s"," ");
		}
		else
		{
	        snprintf(NameIn,sizeof(NameIn),"%s",Name);
		}

		for(int Teller2=8;Teller2>=ScorePlace;Teller2--)
  			saveData.HighScores[GameType][Teller2+1] = saveData.HighScores[GameType][Teller2];
        if((strcmp(NameIn," ") == 0))
        	snprintf(saveData.HighScores[GameType][ScorePlace].PName,sizeof(saveData.HighScores[GameType][ScorePlace].PName),"%s","player");
        else
            snprintf(saveData.HighScores[GameType][ScorePlace].PName,sizeof(saveData.HighScores[GameType][ScorePlace].PName),"%s",NameIn);
        saveData.HighScores[GameType][ScorePlace].PScore = Score;
		SaveHighScores();
	}
}
