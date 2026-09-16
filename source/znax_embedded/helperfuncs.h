#ifndef helperfuncs_h
#define helperfuncs_h

#include <stdint.h>

void preloadImages(void);
uint8_t currentSkin(void);
uint32_t getMillis(void);
void fillScreen(uint16_t color);
void fillRect(int x, int y, int w, int h, uint16_t color);
void drawRect(int x, int y, int w, int h, uint16_t color);
void printText(int16_t x, int16_t y, const char* str, uint16_t color, uint16_t bg, uint8_t size);
void drawImagePart(int x, int y, int sx, int sy, int w, int h, const uint8_t* data, int dataWidth, bool transparent);
void drawImageTransparent(int x, int y, int w, int h, const uint8_t* data);
void drawImageRLEPart(int x, int y, int sx, int sy, int w, int h, const uint8_t* data, int dataWidth, int dataHeight, bool transparent);
void drawImageRLE(int x, int y, int w, int h, const uint8_t* data);
void drawImageRLETransparent(int x, int y, int w, int h, const uint8_t* data);
void drawBackgroundPart(int x, int y, int w, int h);
#endif
