#include <string.h>
#include "cworldparts.h"
#include "common.h"
#include "defines.h"
#include "helperfuncs.h"
#include "sound.h"
//the blocks are drawn into a strip of the screen when one is open
#include "bandrender.h"

CWorldParts *World;

//What each cell of the board shows right now: Color * 16 + AnimPhase of the block drawn there.
//Only a cell that differs is drawn again. cellDirty: the block has to be drawn again and the
//background under it first, cellClean: the background under it is already there
#define cellDirty -1
#define cellClean -2
static int16_t shownBlock[NrOfCols][NrOfRows];

CBlock* CBlock_Create(int PlayFieldXin,int PlayFieldYin,int ColorIn)
{
    CBlock* Result = (CBlock *) malloc(sizeof(CBlock));
    Result->AnimBase = 0;
    Result->AnimPhase = 0;
    Result->AnimCounter = 0;
    Result->AnimDelayCounter = 0;
    Result->AnimPhases = 1;
    Result->AnimDelay = 2;
    Result->PlayFieldX = PlayFieldXin;
    Result->PlayFieldY = PlayFieldYin;
    Result->Color = ColorIn;
    Result->bNeedToKill = false;
    Result->bSelected = false;
    return Result;
}

void CBlock_Select(CBlock* Block)
{
    Block->AnimPhase = 0;
    Block->AnimBase = 0;
    Block->AnimCounter = 0;
    Block->AnimPhases = 6;
    Block->bSelected = true;
}

void CBlock_DeSelect(CBlock* Block)
{
    Block->AnimPhase = 0;
    Block->AnimBase = 0;
    Block->AnimCounter = 0;
    Block->AnimPhases = 1;
    Block->bSelected=false;
}

void CBlock_Kill(CBlock* Block)
{
    //The last column of the sheet, the grey tile, and the animation is parked on it.
    //AnimPhase was 0 here, which is the plain tile: the cell was drawn with that first and only
    //the frame after it with the grey one, since CBlock_Animate is what puts AnimPhase at
    //AnimBase. The whole rectangle a match clears was painted twice over
    Block->AnimBase = 6;
    Block->AnimPhase = Block->AnimBase;
    Block->AnimCounter = 0;
    Block->AnimPhases = 1;
    Block->bNeedToKill = true;
}

void CBlock_Draw(CBlock* Block)
{
    int Srcx = Block->AnimPhase *TileWidth;
    int Srcy = Block->Color *TileHeight;

    int Dstx = BlockScreenX(Block->PlayFieldX);
    int Dsty = BlockScreenY(Block->PlayFieldY);

    //the blocks image has a column per animphase and a row per color
    drawImagePart(Dstx, Dsty, Srcx, Srcy, TileWidth, TileHeight, imgBlocks, blocksWidth, true);
}

//moves the animation on, once a frame after the block was drawn
void CBlock_Animate(CBlock* Block)
{
    Block->AnimPhase = Block->AnimBase + Block->AnimCounter;
    if(Block->AnimPhase != Block->AnimBase + Block->AnimPhases - 1)
    {
        Block->AnimDelayCounter++;
        if (Block->AnimDelayCounter >= Block->AnimDelay)
        {
            Block->AnimDelayCounter = 0;
            Block->AnimCounter++;
            if (Block->AnimCounter == Block->AnimPhases)
                Block->AnimCounter = 0;
        }
    }
}

void CBlock_Destroy(CBlock* Block)
{
    free(Block);
    Block = NULL;
}

CWorldParts* CWorldParts_Create()
{
    CWorldParts* Result = (CWorldParts*) malloc(sizeof(CWorldParts));
    int X,Y;
    Result->NeedToKillBlocks = false;
    Result->NeedToAddBlocks = false;
    Result->NumSelected = 0;
    Result->SelectedColor = -1;
    Result->KilledX = 0;
    Result->KilledY = 0;
    Result->KilledEndX = -1;
    Result->KilledEndY = -1;
    for(Y=0;Y<NrOfRows;Y++)
        for(X=0;X<NrOfCols;X++)
        {
            Result->Items[X][Y] = NULL;
            shownBlock[X][Y] = cellDirty;
        }
    return Result;
}

void CWorldParts_KillBlocks(CWorldParts* WorldParts)
{
    int X,Y,Teller=0,StartX=NrOfCols,StartY=NrOfRows,EndX=-1,EndY=-1;
    for(Teller=0;Teller<WorldParts->NumSelected;Teller++)
    {
        if(WorldParts->Selects[Teller].X > EndX)
            EndX = WorldParts->Selects[Teller].X;
        if(WorldParts->Selects[Teller].X < StartX)
            StartX = WorldParts->Selects[Teller].X;
        if(WorldParts->Selects[Teller].Y > EndY)
            EndY = WorldParts->Selects[Teller].Y;
        if(WorldParts->Selects[Teller].Y < StartY)
            StartY = WorldParts->Selects[Teller].Y;
    }
    for(Y = StartY;Y<=EndY;Y++)
        for(X=StartX;X<=EndX;X++)
            CBlock_Kill(WorldParts->Items[X][Y]);
    //kept for the screen to paint, and for CWorldParts_AddBlocks: the cells it replaces are
    //these same ones, see CWorldParts_Step
    WorldParts->KilledX = (int8_t)StartX;
    WorldParts->KilledY = (int8_t)StartY;
    WorldParts->KilledEndX = (int8_t)EndX;
    WorldParts->KilledEndY = (int8_t)EndY;
}

int CWorldParts_MovesLeft(CWorldParts* WorldParts)
{
    int count = 0;

    // Iterate over all possible pairs of top-left and bottom-right corners
    for (int x1 = 0; x1 < NrOfCols; x1++)
    {
      for (int y1 = 0; y1 < NrOfRows; y1++)
      {
        for (int x2 = x1 + 1; x2 < NrOfCols; x2++)
        {
          for (int y2 = y1 + 1; y2 < NrOfRows; y2++)
          {
            // Check if the corners are the same
            if ((WorldParts->Items[x1][y1]->Color == WorldParts->Items[x1][y2]->Color) &&
                (WorldParts->Items[x1][y1]->Color == WorldParts->Items[x2][y1]->Color) &&
                (WorldParts->Items[x1][y1]->Color == WorldParts->Items[x2][y2]->Color) &&
                //no lines
                (x2 - x1 > 0) && (y2 - y1 > 0)) 
            {
                count++;
            }
          }
        }
      }
    }

    return count;
}

void CWorldParts_AddBlocks(CWorldParts* WorldParts)
{
    int X,Y;
    for(Y=0;Y<NrOfRows;Y++)
        for(X=0;X<NrOfCols;X++)
            if(WorldParts->Items[X][Y]->bNeedToKill)
            {
                CBlock_Destroy(WorldParts->Items[X][Y]);
                WorldParts->Items[X][Y] = CBlock_Create(X,Y,rand()%NrOfBlockColors);
            }
    movesLeft = CWorldParts_MovesLeft(WorldParts);
}


void CWorldParts_NewGame(CWorldParts* WorldParts)
{
    int X,Y;
    movesLeft = 0;
    while(movesLeft < 10) 
    {
        for(Y=0;Y<NrOfRows;Y++)
            for(X=0;X<NrOfCols;X++)
            {
                if (WorldParts->Items[X][Y])
                    CBlock_Destroy(WorldParts->Items[X][Y]);
                WorldParts->Items[X][Y] = CBlock_Create(X,Y,rand()%NrOfBlockColors);
            }
        movesLeft = CWorldParts_MovesLeft(WorldParts);
    }
    WorldParts->NeedToKillBlocks = false;
    WorldParts->NeedToAddBlocks = false;
    WorldParts->NumSelected = 0;
    WorldParts->SelectedColor = -1;
}

void CWorldParts_Destroy(CWorldParts* WorldParts)
{
    int X,Y;
    for(Y=0;Y<NrOfRows;Y++)
        for(X=0;X<NrOfCols;X++)
            if(WorldParts->Items[X][Y])
                CBlock_Destroy(WorldParts->Items[X][Y]);
}

//The background under the rectangle x,y,w,h was drawn again: the blocks on it are drawn again
//as well. A cell that lies inside it as a whole is clean, one it only partly covers is dirty
//A cell at a time will not do for putting an area back: BlockScreenX is x * TileWidth + 7 + x, so
//the columns sit a pixel apart and a repaint that goes cell by cell leaves those gaps as they were.
//What was over them, an overlay say, stays on the screen. The whole rectangle's background has to
//be painted, which is what CWorldParts_InvalidateRect is paired with
//The part of the board an overlay is hiding, or nothing when w or h is 0.
//A cell it covers whole is not drawn at all: the blocks animate, so without this every frame
//drew the tiles under READY or GO and then drew the overlay again on top of them, and with the
//art read from a card each of those is slow enough to watch. The cells keep whatever they last
//showed, so they are wrong underneath; CWorldParts_InvalidateRect puts them right when the
//overlay goes, which is what happens anyway
static int coveredX = 0, coveredY = 0, coveredW = 0, coveredH = 0;

void CWorldParts_SetCovered(int x, int y, int w, int h)
{
    coveredX = x;
    coveredY = y;
    coveredW = w;
    coveredH = h;
}

//true when the overlay hides every pixel of this cell
static bool CellCovered(int X, int Y)
{
    if ((coveredW <= 0) || (coveredH <= 0))
        return false;
    const int cx = BlockScreenX(X);
    const int cy = BlockScreenY(Y);
    return (cx >= coveredX) && (cx + TileWidth <= coveredX + coveredW) &&
           (cy >= coveredY) && (cy + TileHeight <= coveredY + coveredH);
}

void CWorldParts_InvalidateRect(int x, int y, int w, int h)
{
    int X,Y;
    for(Y=0;Y<NrOfRows;Y++)
        for(X=0;X<NrOfCols;X++)
        {
            const int cx = BlockScreenX(X);
            const int cy = BlockScreenY(Y);
            if ((cx + TileWidth <= x) || (cx >= x + w) || (cy + TileHeight <= y) || (cy >= y + h))
                continue;
            if ((cx >= x) && (cx + TileWidth <= x + w) && (cy >= y) && (cy + TileHeight <= y + h))
                shownBlock[X][Y] = cellClean;
            else if (shownBlock[X][Y] != cellClean)
                shownBlock[X][Y] = cellDirty;
        }
}

//The blocks of the cells that show bare background, drawn into the strip that is open. That is
//the cells CWorldParts_InvalidateRect found the rectangle holds whole, and the strip has their
//background in it already, so the block goes straight on top of it and nothing is ever shown
//half painted. A cell is eight pixels tall and the board starts at row 19, so a cell falls in
//two strips: it is drawn into both and the clipping in bandrender.cpp takes the right half
void CWorldParts_DrawCleanCells(CWorldParts* WorldParts)
{
    //On a device that has no strips the caller painted the rectangle straight to the screen and
    //calls this between the background and the overlay, so every row of the board is in it.
    //BandRender_StripH is 0 there, which would have left the whole board out
    const bool strip = BandRender_Drawing();
    const int top = strip ? BandRender_StripY() : 0;
    const int bottom = strip ? (top + BandRender_StripH()) : WINDOW_HEIGHT;
    int X,Y;
    for(Y=0;Y<NrOfRows;Y++)
    {
        const int cy = BlockScreenY(Y);
        //the rows of the board this strip cannot hold cost nothing more than this test. Without
        //it every cell would read a sixteen colour palette before finding it has nothing to draw
        if ((cy + TileHeight <= top) || (cy >= bottom))
            continue;
        for(X=0;X<NrOfCols;X++)
            //Dirty as well as clean. A cell the rectangle only partly covers is marked dirty,
            //meaning it wants its background painting before its block - but inside a strip the
            //background of the whole rectangle is already there, so it is drawn like any other.
            //Left to the ordinary path instead, those cells were never put back and the board
            //came up with a ring of bare background one cell wide around where the overlay was
            if ((shownBlock[X][Y] == cellClean) || (shownBlock[X][Y] == cellDirty))
                CBlock_Draw(WorldParts->Items[X][Y]);
    }
}

//Those cells now show their block, which the strips have all been sent. Kept apart from the
//drawing because a cell falls in two strips: it is only finished once the last strip has gone
void CWorldParts_MarkCleanDrawn(CWorldParts* WorldParts)
{
    int X,Y;
    for(Y=0;Y<NrOfRows;Y++)
        for(X=0;X<NrOfCols;X++)
            //the same two the pass drew, see CWorldParts_DrawCleanCells
            if ((shownBlock[X][Y] == cellClean) || (shownBlock[X][Y] == cellDirty))
            {
                CBlock* Block = WorldParts->Items[X][Y];
                shownBlock[X][Y] = (int16_t)(Block->Color * 16 + Block->AnimPhase);
            }
}

//Draws the blocks that differ from what their cell shows and moves the animations on. Returns if
//anything was drawn, CursorCellDrawn is set when the cell at CursorX,CursorY was
//Both of these change a whole rectangle of the board in one go, so the rectangle is reported and
//the screen paints it in one pass. Cell by cell the loop below paints each of them on its own, and
//with the art on a card each one reads the piece of background it sits on by itself, eight reads a
//cell: the blocks could be watched turning grey one after another, a fifth of a second for a four
//by four match and over a second for the biggest. Only one of the two can fire in a call, the kill
//sets the timer the replace waits on
bool CWorldParts_Step(CWorldParts* WorldParts, int* x, int* y, int* w, int* h)
{
    bool changed = false;
    if(WorldParts->NeedToKillBlocks && (WorldParts->Time < getMillis()))
    {
        CWorldParts_KillBlocks(WorldParts);
        WorldParts->NeedToKillBlocks = false;
        WorldParts->NeedToAddBlocks = true;
        WorldParts->Time = getMillis() + 350;
        SelectMusic(musNone, 0);
        SelectMusic(musClear, 0);
        changed = true;
    }

    if (WorldParts->NeedToAddBlocks && (WorldParts->Time < getMillis()))
    {
        CWorldParts_AddBlocks(WorldParts);
        WorldParts->NeedToAddBlocks = false;
        WorldParts->NumSelected = 0;
        changed = true;
    }

    if (changed && (WorldParts->KilledEndX >= WorldParts->KilledX))
    {
        //the columns sit a pixel apart, see BlockScreenX, and the rectangle takes in the gaps
        //between them: a repaint that leaves those out is what CWorldParts_InvalidateRect warns of
        const int px = BlockScreenX(WorldParts->KilledX);
        const int py = BlockScreenY(WorldParts->KilledY);
        *x = px;
        *y = py;
        *w = BlockScreenX(WorldParts->KilledEndX) + TileWidth - px;
        *h = BlockScreenY(WorldParts->KilledEndY) + TileHeight - py;
        return true;
    }
    return false;
}

bool CWorldParts_Draw(CWorldParts* WorldParts, int CursorX, int CursorY, bool* CursorCellDrawn)
{
    //for when nobody asked for the rectangle, which is while an overlay is up: it lies over the
    //board and the rectangle would be painted on top of it
    int kx, ky, kw, kh;
    CWorldParts_Step(WorldParts, &kx, &ky, &kw, &kh);

    bool drawn = false;
    int X,Y;
    for(Y=0;Y<NrOfRows;Y++)
        for(X=0;X<NrOfCols;X++)
        {
            CBlock* Block = WorldParts->Items[X][Y];
            const int16_t shown = (int16_t)(Block->Color * 16 + Block->AnimPhase);
            //nothing of this cell can be seen, so drawing it would only be undone by the overlay
            if (CellCovered(X, Y))
            {
                CBlock_Animate(Block);
                continue;
            }
            if (shown != shownBlock[X][Y])
            {
                //the blocks have transparent corners, what was under them has to go first
                if (shownBlock[X][Y] != cellClean)
                    drawBackgroundPart(BlockScreenX(X), BlockScreenY(Y), TileWidth, TileHeight);
                CBlock_Draw(Block);
                shownBlock[X][Y] = shown;
                drawn = true;
                if ((X == CursorX) && (Y == CursorY))
                    *CursorCellDrawn = true;
            }
            CBlock_Animate(Block);
        }
    return drawn;
}

void CWorldParts_DeSelect(CWorldParts* WorldParts, bool PlaySound)
{
    playErrorSound();
    int Teller = 0;
    for(Teller = 0;Teller<WorldParts->NumSelected;Teller++)
        CBlock_DeSelect(WorldParts->Items[WorldParts->Selects[Teller].X][WorldParts->Selects[Teller].Y]);
    WorldParts->NumSelected = 0;
    WorldParts->SelectedColor = -1;
}

long CWorldParts_Select(CWorldParts* WorldParts, int PlayFieldX,int PlayFieldY)
{

    int tmpScore = 0,NumEqualY=0,NumEqualX = 0,Teller1=0,Teller2=0,StartX=NrOfCols,StartY=NrOfRows,EndX=-1,EndY=-1;;
    if(!WorldParts->NeedToKillBlocks && !WorldParts->NeedToAddBlocks)
    {
        if(!WorldParts->Items[PlayFieldX][PlayFieldY]->bSelected)
        {

            if (WorldParts->NumSelected == 0)
            {
                CBlock_Select(WorldParts->Items[PlayFieldX][PlayFieldY]);
                WorldParts->SelectedColor = WorldParts->Items[PlayFieldX][PlayFieldY]->Color;
                WorldParts->Selects[WorldParts->NumSelected].X = PlayFieldX;
                WorldParts->Selects[WorldParts->NumSelected].Y = PlayFieldY;
                WorldParts->NumSelected++;
                playMenuSelectSound();

            }
            else
            {
                if(WorldParts->Items[PlayFieldX][PlayFieldY]->Color == WorldParts->SelectedColor)
                {
                    CBlock_Select(WorldParts->Items[PlayFieldX][PlayFieldY]);
                    WorldParts->Selects[WorldParts->NumSelected].X = PlayFieldX;
                    WorldParts->Selects[WorldParts->NumSelected].Y = PlayFieldY;
                    WorldParts->NumSelected++;
                    if(WorldParts->NumSelected > 2)
                    {
                        for(Teller1=0;Teller1 < WorldParts->NumSelected;Teller1++)
                        {
                            for(Teller2=0;Teller2<WorldParts->NumSelected;Teller2++)
                            if(Teller1 != Teller2)
                            {
                                if(WorldParts->Selects[Teller1].X == WorldParts->Selects[Teller2].X)
                                    NumEqualX++;
                                if(WorldParts->Selects[Teller1].Y == WorldParts->Selects[Teller2].Y)
                                    NumEqualY++;
                            }
                        }
                        if((NumEqualX > 4) || (NumEqualY > 4))
                            CWorldParts_DeSelect(WorldParts, true);
                    }


                    if(WorldParts->NumSelected == 3)
                        if (!(((NumEqualX == 2) && (NumEqualY == 2)) ))
                            CWorldParts_DeSelect(WorldParts, true);

                    if(WorldParts->NumSelected == 4)
                    {
                        if((NumEqualX == 4) && (NumEqualY == 4))
                        {
                            for(Teller1=0;Teller1<WorldParts->NumSelected;Teller1++)
                            {
                                if(WorldParts->Selects[Teller1].X > EndX)
                                    EndX = WorldParts->Selects[Teller1].X;
                                if(WorldParts->Selects[Teller1].X < StartX)
                                    StartX = WorldParts->Selects[Teller1].X;
                                if(WorldParts->Selects[Teller1].Y > EndY)
                                    EndY = WorldParts->Selects[Teller1].Y;
                                if(WorldParts->Selects[Teller1].Y < StartY)
                                    StartY = WorldParts->Selects[Teller1].Y;
                            }
                            tmpScore = (EndY-StartY+1) * (EndX-StartX +1) * 100;
                            WorldParts->NeedToKillBlocks = true;
                            WorldParts->Time = getMillis() + 350;
                        }
                        else
                            CWorldParts_DeSelect(WorldParts, true);
                    }

                    if(((WorldParts->NumSelected <= 4) && (WorldParts->NumSelected > 0)) )
                        playMenuSelectSound();
                }
                else
                    CWorldParts_DeSelect(WorldParts, true);
            }

        }
        else
            CWorldParts_DeSelect(WorldParts, true);
    }
    return tmpScore;
}
