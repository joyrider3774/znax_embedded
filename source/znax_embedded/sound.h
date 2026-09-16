#ifndef SOUND_H
#define SOUND_H

#include <stdint.h>

#define musNone 0
#define musStart 1
#define musWinner 2
#define musClear 3
#define mus5Min 4
#define mus3Min 5
#define musGo 6 
#define musReady 7



void playSelectSound();
void playErrorSound();
void playGameAction();
void playMenuSelectSound();
void playMenuBackSound();
void playMenuAcknowlege();
void playThreeSound();
void playTwoSound();
void playOneSound();

void initSound();
void SelectMusic(uint8_t musicFile, uint8_t loop);
void initMusic();
void playTickSound();
void setMusicOn(uint8_t value);
void setSoundOn(uint8_t value);
void musicTimer();
void processSound();
uint8_t isMusicOn();
uint8_t isSoundOn();

#endif