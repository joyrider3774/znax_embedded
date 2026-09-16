#ifndef GAMEFUNCS_H
#define GAMEFUNCS_H

#include <stdint.h>

void DrawGameScreen(bool ShowCursor, const uint8_t* Overlay, int OverlayWidth, int OverlayHeight);
void SaveHighScores();
void LoadHighScores();
char chr(int ascii);
int ord(char chr);

#endif
