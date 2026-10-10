#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "common.h"
#include "defines.h"
#include "gamefuncs.h"
#include "helperfuncs.h"
#include "cardimages.h"
#include "cworldparts.h"
//the game screen is painted a strip at a time so nothing is seen half drawn
#include "bandrender.h"

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
        snprintf(Text,sizeof(Text),"T:%02d:%02d     S:%d",Timer/60,Timer%60,(int)Score);
    else
    {
        if(GameType == Relative)
            snprintf(Text,sizeof(Text),"T:%02d:%02d+%03d S:%d+%d",Timer/60,Timer%60,AddToScore/400,(int)Score, AddToScore);
        else
            snprintf(Text,sizeof(Text),"T:%02d:%02d     S:%d+%d",Timer/60,Timer%60,(int)Score, AddToScore);
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

//Paints a rectangle of the game screen in one pass: the piece of background and the blocks of the
//cells the rectangle holds whole are put together in memory and sent as strips, so what appears
//on the display is the finished picture. Painted the plain way the background goes down first and
//really is on the display for a moment, which is the rectangle of bare background that could be
//seen where READY and GO had been, and the flicker under a cell that is drawn again.
//The cells the rectangle only partly covers are left dirty for CWorldParts_Draw, which paints
//their background itself. Without a strip buffer this is the plain way, see bandrender.h
//Paints a rectangle of the game screen, and an overlay on top of it when one is given, in one
//pass: the background, the blocks and the overlay all go into the strip buffer and the strip is
//sent once it holds the finished picture.
//The overlay belongs in the same pass as the erase. Done separately the display showed the old
//overlay being wiped away, then the board coming back a strip at a time, and only then the new
//overlay drawn on top of it a row at a time - a third of a second of visible redrawing between
//READY and GO once the art came off a card rather than out of flash
static void PaintGameRect(int x, int y, int w, int h,
                          const uint8_t* overlay, int ox, int oy, int ow, int oh)
{
    CWorldParts_InvalidateRect(x, y, w, h);
    if (!BandRender_Begin(imgBackground, (int16_t)x, (int16_t)y, (int16_t)w, (int16_t)h))
    {
        drawBackgroundPart(x, y, w, h);
        if (overlay)
            drawImageRLETransparent(ox, oy, ow, oh, overlay);
        return;
    }
    while (BandRender_Next())
    {
        CWorldParts_DrawCleanCells(World);
        //into the same strip, clipped to it: the drawing calls go to the strip while one is open
        if (overlay)
            drawImageRLETransparent(ox, oy, ow, oh, overlay);
    }
    CWorldParts_MarkCleanDrawn(World);
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
    //What the overlay hides is not drawn: the blocks animate, so otherwise every frame put the
    //tiles back under READY or GO and then drew the overlay over them again
    if (Overlay)
        CWorldParts_SetCovered((WINDOW_WIDTH - OverlayWidth) >> 1, (WINDOW_HEIGHT - OverlayHeight) >> 1,
                               OverlayWidth, OverlayHeight);
    else
        CWorldParts_SetCovered(0, 0, 0, 0);

    bool drawn = false;
    if (needRedraw)
    {
        needRedraw = 0;
#if CARDIMAGES
        //a screen of its own is about to be drawn, so what the one before it cached can go
        CardImages_Reset();
#endif
        // the background and the blocks on it, the cursor, the text and the overlay come below
        PaintGameRect(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, NULL, 0, 0, 0, 0);
        cursorShown = false;
        statusShown = false;
        shownOverlay = NULL;
    }

    // An overlay that goes away leaves the background and the blocks under it to be drawn again,
    // and the one that replaces it goes on in the same pass. The rectangle covers both of them,
    // so READY is gone and GO is up by the time anything is sent to the display
    if (shownOverlay && (shownOverlay != Overlay))
    {
        const int nx = Overlay ? ((WINDOW_WIDTH - OverlayWidth) >> 1) : 0;
        const int ny = Overlay ? ((WINDOW_HEIGHT - OverlayHeight) >> 1) : 0;
        int rx = shownOverlayX, ry = shownOverlayY;
        int rr = shownOverlayX + shownOverlayWidth, rb = shownOverlayY + shownOverlayHeight;
        if (Overlay)
        {
            if (nx < rx) rx = nx;
            if (ny < ry) ry = ny;
            if (nx + OverlayWidth > rr) rr = nx + OverlayWidth;
            if (ny + OverlayHeight > rb) rb = ny + OverlayHeight;
        }
        PaintGameRect(rx, ry, rr - rx, rb - ry, Overlay, nx, ny, OverlayWidth, OverlayHeight);
        if (cursorShown && CellInRect(shownCursor.X, shownCursor.Y, rx, ry, rr - rx, rb - ry))
            cursorShown = false;
        if (Overlay)
        {
            //it is up already, so the test further down leaves it alone
            shownOverlayX = nx;
            shownOverlayY = ny;
            shownOverlayWidth = OverlayWidth;
            shownOverlayHeight = OverlayHeight;
            shownOverlay = Overlay;
        }
        else
            shownOverlay = NULL;
        drawn = true;
    }

    // the cursor lies over its block, when it moves or goes both go and the block is drawn again
    SPoint position = Selector->CurrentPoint;
    if (cursorShown && (!ShowCursor || (position.X != shownCursor.X) || (position.Y != shownCursor.Y)))
    {
        PaintGameRect(BlockScreenX(shownCursor.X), BlockScreenY(shownCursor.Y), cursorWidth, cursorHeight, NULL, 0, 0, 0, 0);
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

    // The overlay is drawn when it is a new one, and then left alone. It used to be drawn again
    // whenever anything had been drawn, but drawn is true for a block changing anywhere on the
    // board and the blocks animate, so that was every frame: the picture was read from the card
    // and put up again thirty times a second, and since a big one is read a row at a time it was
    // visibly wiping itself in, clearing, and starting over. Nothing can draw inside its
    // rectangle now - CWorldParts_SetCovered keeps the cells under it from being drawn, the
    // cursor is not shown while one is up, and the status bar is above it - so once is enough
    if (Overlay && (Overlay != shownOverlay))
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
