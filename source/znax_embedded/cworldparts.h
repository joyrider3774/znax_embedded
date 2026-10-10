#include <stdint.h>

#ifndef CWORLDPARTS_H
#define CWORLDPARTS_H

#include "common.h"
#include "defines.h"

//Every one of these is small and there are NrOfCols * NrOfRows of them, 169, so the width of the
//fields is what the board costs: as ints it was 40 bytes each and 6760 in all, which with the art
//read from a card (CARDARENA) left the heap short and the board came up with blocks missing.
//A colour is 0 to 6, an animation has at most 6 phases and its delay is 2, and the playfield is
//13 by 13, so a byte holds any of them with room to spare
struct CBlock
{
    uint8_t Color,AnimCounter,AnimBase,AnimDelay,AnimDelayCounter,AnimPhases,AnimPhase;
    uint8_t PlayFieldX,PlayFieldY;
    bool bNeedToKill : 1;
    bool bSelected : 1;
};

typedef struct CBlock CBlock;

CBlock* CBlock_Create(int PlayFieldXin,int PlayFieldYin,int ColorIn);
void CBlock_Select(CBlock* Block);
void CBlock_DeSelect(CBlock* Block);
void CBlock_Kill(CBlock* Block);
void CBlock_Draw(CBlock* Block);
void CBlock_Animate(CBlock* Block);
int CBlock_GetColor(CBlock* Block);
bool CBlock_IsSelected(CBlock* Block);
bool CBlock_NeedToKill(CBlock* Block);
void CBlock_Destroy(CBlock* Block);

struct CWorldParts
{
    CBlock *Items[NrOfCols][NrOfRows];
    SPoint Selects [4];
    int NumSelected,SelectedColor;
    uint32_t Time;
    //the rectangle of the board the blocks a match cleared cover, in cells, which is both the
    //rectangle that turns grey and the one that is filled with new blocks 350ms later
    int8_t KilledX,KilledY,KilledEndX,KilledEndY;
    bool NeedToKillBlocks,NeedToAddBlocks;
};
typedef struct CWorldParts CWorldParts;


CWorldParts* CWorldParts_Create();
void CWorldParts_Destroy(CWorldParts* WorldParts);
void CWorldParts_KillBlocks(CWorldParts* WorldParts);
void CWorldParts_AddBlocks(CWorldParts* WorldParts);
void CWorldParts_NewGame(CWorldParts* WorldParts);
bool CWorldParts_Draw(CWorldParts* WorldParts, int CursorX, int CursorY, bool* CursorCellDrawn);
//Turns the blocks a match cleared grey, and 350ms later puts new ones in their place, when either
//is due. Returns true when one of them happened, with the rectangle of the screen those blocks
//cover, for a caller that can paint it in one pass. See the note by it in cworldparts.cpp
bool CWorldParts_Step(CWorldParts* WorldParts, int* x, int* y, int* w, int* h);
void CWorldParts_InvalidateRect(int x, int y, int w, int h);
//The rectangle an overlay is hiding, so the cells under it are left alone while it is up. 0 for
//w or h means nothing is hidden. See the note by it in cworldparts.cpp
void CWorldParts_SetCovered(int x, int y, int w, int h);
//For painting an area of the board in one pass, see bandrender.h. After InvalidateRect marked
//which cells the area holds whole, DrawCleanCells puts their blocks into the strip that is open
//and MarkCleanDrawn records them as drawn once every strip has gone out
void CWorldParts_DrawCleanCells(CWorldParts* WorldParts);
void CWorldParts_MarkCleanDrawn(CWorldParts* WorldParts);
void CWorldParts_DeSelect(CWorldParts* WorldParts, bool PlaySound);
long CWorldParts_Select(CWorldParts* WorldParts, int PlayFieldX,int PlayFieldY);
int CWorldParts_MovesLeft(CWorldParts* WorldParts);

extern CWorldParts *World;

#endif
