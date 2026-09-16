#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "common.h"
#include "defines.h"
#include "gamefuncs.h"
#include "helperfuncs.h"

#define SAVE_MAGIC 0xDADA

//everything saved has to fit in what the platform stores, and keep the layout of the ESPboy's saves
static_assert(sizeof(SHighScore) == 16, "a high score is no longer laid out as the ESPboy saved it");
static_assert(sizeof(SaveData) <= PLATFORM_STORAGE_SIZE, "the save state does not fit in PLATFORM_STORAGE_SIZE");

uint8_t calcCRC(void *data, size_t len) {
  uint8_t crc = 0;
  uint8_t *ptr = (uint8_t *)data;
  for (size_t i = 0; i < len; i++) {
    crc ^= ptr[i];
    for (uint8_t j = 0; j < 8; j++) {
      if (crc & 0x80)
        crc = (crc << 1) ^ 0x07;
      else
        crc <<= 1;
    }
  }
  return crc;
}

void LoadHighScores()
{
    Platform_StorageRead(0, (uint8_t*)&saveData, sizeof(SaveData));
    //needs to be -uint8t size because of crc not included in calculation
    uint8_t crc = calcCRC(&saveData, sizeof(SaveData) - sizeof(uint8_t));
    if (saveData.magic != SAVE_MAGIC || saveData.crc != crc)
    {
        Platform_Log("save state invalid, loading defaults\n");
        memset(&saveData, 0, sizeof(SaveData));
        saveData.magic = SAVE_MAGIC;

        for (int Teller = 0;Teller<10;Teller++)
 	    {
 	        sprintf(saveData.HighScores[Fixed][Teller].PName,"%s","joyrider");
 	        sprintf(saveData.HighScores[Relative][Teller].PName,"%s","joyrider");
 	        saveData.HighScores[Fixed][Teller].PScore = 0;
 	        saveData.HighScores[Relative][Teller].PScore = 0;
 	    }
    }
    else
    {
        Platform_Log("save state valid, loaded scores\n");
    }
}

void SaveHighScores()
{
    saveData.crc = calcCRC(&saveData, sizeof(SaveData) - sizeof(uint8_t));
    Platform_StorageWrite(0, (const uint8_t*)&saveData, sizeof(SaveData));
    Platform_Log("saved, crc: 0x%02X\n", saveData.crc);
}

// What the game screen (ReadyGo, Game and TimeOver) shows right now. Only what differs is drawn
// again, so straight to the display (SCREENBUFFER 0) nothing flickers and little is sent. With a
// buffer the buffer keeps the last frame, so the same holds there. needRedraw draws it all again.
// The blocks keep what their cells show themselves, see CWorldParts_Draw
static bool cursorShown = false;
static SPoint shownCursor;
static bool statusShown = false;
static char shownStatus[48];
static const uint8_t* shownOverlay = NULL;
static int shownOverlayX, shownOverlayY, shownOverlayWidth, shownOverlayHeight;

//the status bar at the top, only drawn when its text changed. Returns if it was drawn
static bool DrawStatusBar()
{
    char Text[48];
    if(AddToScore == 0)
        snprintf(Text,sizeof(Text),"T:%02d:%02d     S:%ld",Timer/60,Timer%60,(long)Score);
    else
    {
        if(GameType == Relative)
            snprintf(Text,sizeof(Text),"T:%02d:%02d+%03d S:%ld+%d",Timer/60,Timer%60,AddToScore/400,(long)Score, AddToScore);
        else
            snprintf(Text,sizeof(Text),"T:%02d:%02d     S:%ld+%d",Timer/60,Timer%60,(long)Score, AddToScore);
    }

    if (statusShown && (strcmp(Text, shownStatus) == 0))
        return false;
    const int x = (int)(10*SCALE);
    const int y = (int)(16*SCALE);
    //the text is 8 pixels high and runs to the right edge at most
    drawBackgroundPart(x, y, WINDOW_WIDTH - x, 8);
    printText(x, y, Text, ColorStatusText, ColorStatusText, 1);
    strcpy(shownStatus, Text);
    statusShown = true;
    return true;
}

//true when the cell of the block at playfield X,Y and the rectangle overlap
static bool CellInRect(int X, int Y, int x, int y, int w, int h)
{
    const int cx = BlockScreenX(X);
    const int cy = BlockScreenY(Y);
    return !((cx + TileWidth <= x) || (cx >= x + w) || (cy + TileHeight <= y) || (cy >= y + h));
}

//Brings the game screen up to the game's state: the board, the cursor when ShowCursor is set, the
//status bar and Overlay (NULL for none) in the middle of the screen
void DrawGameScreen(bool ShowCursor, const uint8_t* Overlay, int OverlayWidth, int OverlayHeight)
{
    bool drawn = false;
    if (needRedraw)
    {
        needRedraw = 0;
        // the bare background shows no block, cursor, text or overlay, what is there is drawn on it below
        drawImageRLE(0, 0, fullScreenWidth, fullScreenHeight, imgBackground);
        CWorldParts_InvalidateRect(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
        cursorShown = false;
        statusShown = false;
        shownOverlay = NULL;
    }

    // an overlay that goes away leaves the background and the blocks under it to be drawn again
    if (shownOverlay && (shownOverlay != Overlay))
    {
        drawBackgroundPart(shownOverlayX, shownOverlayY, shownOverlayWidth, shownOverlayHeight);
        CWorldParts_InvalidateRect(shownOverlayX, shownOverlayY, shownOverlayWidth, shownOverlayHeight);
        if (cursorShown && CellInRect(shownCursor.X, shownCursor.Y, shownOverlayX, shownOverlayY, shownOverlayWidth, shownOverlayHeight))
            cursorShown = false;
        shownOverlay = NULL;
        drawn = true;
    }

    // the cursor lies over its block, when it moves or goes both go and the block is drawn again
    SPoint position = Selector->CurrentPoint;
    if (cursorShown && (!ShowCursor || (position.X != shownCursor.X) || (position.Y != shownCursor.Y)))
    {
        drawBackgroundPart(BlockScreenX(shownCursor.X), BlockScreenY(shownCursor.Y), cursorWidth, cursorHeight);
        CWorldParts_InvalidateRect(BlockScreenX(shownCursor.X), BlockScreenY(shownCursor.Y), cursorWidth, cursorHeight);
        cursorShown = false;
        drawn = true;
    }

    bool cursorCellDrawn = false;
    drawn |= CWorldParts_Draw(World, position.X, position.Y, &cursorCellDrawn);

    if (ShowCursor && (!cursorShown || cursorCellDrawn))
    {
        CSelector_Draw(Selector);
        shownCursor = position;
        cursorShown = true;
        drawn = true;
    }

    DrawStatusBar();

    // the overlay lies over the board, it is drawn again when something under it may have been
    if (Overlay && ((Overlay != shownOverlay) || drawn))
    {
        shownOverlayX = (WINDOW_WIDTH - OverlayWidth) >> 1;
        shownOverlayY = (WINDOW_HEIGHT - OverlayHeight) >> 1;
        shownOverlayWidth = OverlayWidth;
        shownOverlayHeight = OverlayHeight;
        drawImageRLETransparent(shownOverlayX, shownOverlayY, OverlayWidth, OverlayHeight, Overlay);
        shownOverlay = Overlay;
    }
}

char chr(int ascii)
{
	return((char)ascii);
}

int ord(char chr)
{
	return((int)chr);
}
