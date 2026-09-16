#include <string.h>
#include "cworldparts.h"
#include "common.h"
#include "defines.h"
#include "helperfuncs.h"
#include "sound.h"

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
    Block->AnimPhase = 0;
    Block->AnimBase = 6;
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

//Draws the blocks that differ from what their cell shows and moves the animations on. Returns if
//anything was drawn, CursorCellDrawn is set when the cell at CursorX,CursorY was
bool CWorldParts_Draw(CWorldParts* WorldParts, int CursorX, int CursorY, bool* CursorCellDrawn)
{
    if(WorldParts->NeedToKillBlocks && (WorldParts->Time < getMillis()))
    {
        CWorldParts_KillBlocks(WorldParts);
        WorldParts->NeedToKillBlocks = false;
        WorldParts->NeedToAddBlocks = true;
        WorldParts->Time = getMillis() + 350;
        SelectMusic(musNone, 0);
        SelectMusic(musClear, 0);
    }

    if (WorldParts->NeedToAddBlocks && (WorldParts->Time < getMillis()))
    {
        CWorldParts_AddBlocks(WorldParts);
        WorldParts->NeedToAddBlocks = false;
        WorldParts->NumSelected = 0;
    }

    bool drawn = false;
    int X,Y;
    for(Y=0;Y<NrOfRows;Y++)
        for(X=0;X<NrOfCols;X++)
        {
            CBlock* Block = WorldParts->Items[X][Y];
            const int16_t shown = (int16_t)(Block->Color * 16 + Block->AnimPhase);
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
