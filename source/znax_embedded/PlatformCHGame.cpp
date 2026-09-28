//Platform.h for the CHGame, Kevin Bates' CH32X035 handheld. Nothing but the Arduino core of its
//board package (github.com/bateske/CH32SerialBoot) is used, with the core's SPI library for the
//display and SdFat for the save file:
//  display  ST7735S 128x128 on SPI1, chip select PA4, data/command PB0, reset PB12
//  buttons  one GPIO each, pulled up and low while held
//  sound    the buzzer on PB10 through the core's tone(), which drives it from TIM3
//  saves    a file on the microSD card (chip select PB11), in a folder named after the game
//
//The display numbers are the ones CHGfx (github.com/bateske/CHGfx) uses for this panel, which is
//the library written for this board: MADCTL 0xC8 with the picture starting at column 2, row 3.
//
//Buttons: d-pad, A, B, SELECT = left side button, START = right side button.
//
//Two traps of this chip, both of which CHGfx documents and both of which this file stays clear of
//by going through the core rather than the registers:
//  - GPIOB's CFGHR is write only. Reading it back does not return what was written, so the core
//    keeps a RAM shadow and a raw write behind its back is undone by the next pinMode on any pin
//    of that half of the port. Every pin here is set through pinMode and digitalWrite
//  - the reset line drifts below its threshold if it is left alone for about a second, and the
//    panel then takes that for a reset and blanks to white while the game carries on drawing into
//    it. It is driven high again on every window change, which is four instructions against the
//    eleven SPI transfers that follow it

#include "Platform.h"
//every platform's source sits in the sketch folder, only the one being built compiles
#ifdef PLATFORM_CHGAME

#include <stdarg.h>
#include <stdio.h>
#include <malloc.h>
#include <Arduino.h>
#include <SPI.h>
#include "PlatformGamebuinoFont.h"

//the save file needs a card reader. Without the library the game still runs, it just does not
//keep anything, and the build says so rather than failing
#if defined(__has_include)
  #if __has_include(<SdFat.h>)
  #define CHGAME_HAS_SDFAT 1
  #endif
#endif
#ifdef CHGAME_HAS_SDFAT
#include <SdFat.h>
#endif

//the game's 128x128 screen is the whole display, there is no border to keep black
#define DISPLAY_WIDTH 128
#define DISPLAY_HEIGHT 128
//where this panel's first pixel sits in the controller's memory, from CHGfx
#define DISPLAY_OFFSET_X 2
#define DISPLAY_OFFSET_Y 3

//24 MHz, the SPI1 clock divided by two. The panel is only ever written to, so the transfers ask
//for transmit only: the core's block transfer otherwise writes what comes back over the buffer
//it was given
#define DISPLAY_SPI_HZ 24000000
#define DISPLAY_SPI_SETTINGS SPISettings(DISPLAY_SPI_HZ, MSBFIRST, SPI_MODE0, SPI_TRANSMITONLY)
//the card is happy slower, and is talked to while no display transaction is open
#define SD_SPI_HZ 12000000

static PlatformCHGameDisplay display;
PlatformDisplay& platformDisplay = display;

#if SCREENBUFFER
//the whole frame is drawn in here and sent to the display once at the end of Game_Loop
PlatformBuffer screenBuffer;
//the two colours a 1 bpp frame is shown in, high byte first as the display takes them
static uint8_t bufferSetColor[2] = { 0xFF, 0xFF }, bufferClearColor[2] = { 0x00, 0x00 };
#endif

//Pixels are handed to the SPI in RAM lumps rather than a byte at a time: a call per byte costs
//more than the byte takes on the wire. It also keeps what is sent away from the caller's data,
//which may be in flash and cannot be written over
#define SPI_CHUNK_PIXELS 64
static uint8_t spiChunk[SPI_CHUNK_PIXELS * 2];

static void PaintStack(void);
static void StorageInit(const char* appName);

// ===========================================================================
// Program start
// ===========================================================================

//the Arduino core starts the program here
void setup()
{
	Game_Setup();
}

void loop()
{
	Game_Loop();
}

// ===========================================================================
// SPI and the panel's lines
// ===========================================================================

//sends bytes that are already in RAM. The buffer is the library's to write over, which with
//transmit only it does not, but nothing here hands it anything it would mind losing
static inline void SpiSend(uint8_t* data, size_t length)
{
	SPI.transfer(data, length);
}

static inline void SpiSendByte(uint8_t value)
{
	SPI.transfer(value);
}

//see the note at the top: the panel takes a drifting reset line for a real reset
static inline void ResetDriveHigh(void)
{
	digitalWrite(PIN_LCD_RST, HIGH);
}

static void WriteCommand(uint8_t command)
{
	digitalWrite(PIN_LCD_DC, LOW);
	SpiSendByte(command);
	digitalWrite(PIN_LCD_DC, HIGH);
}

// ===========================================================================
// Display
// ===========================================================================

#define ST7735_DELAY 0x80

//The init CHGfx uses for this panel: command, argument count (with ST7735_DELAY a delay in ms
//follows the arguments), arguments. The frame rate triplet 0x05,0x3A,0x3A scans the glass
//faster than the usual 0x01,0x2C,0x2D, MADCTL 0xC8 is the 1.44" green tab's rotation 0
static const uint8_t displayResetCommands[] = {
	2,
	0x01, ST7735_DELAY, 150,        //software reset
	0x11, ST7735_DELAY, 255,        //out of sleep mode
};
static const uint8_t displayInitCommands[] = {
	17,
	0xB1, 3, 0x05, 0x3A, 0x3A,      //frame rate, normal mode
	0xB2, 3, 0x05, 0x3A, 0x3A,      //frame rate, idle mode
	0xB3, 6, 0x05, 0x3A, 0x3A, 0x05, 0x3A, 0x3A,  //frame rate, partial mode
	0xB4, 1, 0x07,                  //no inversion
	0xC0, 3, 0xA2, 0x02, 0x84,      //power control
	0xC1, 1, 0xC5,
	0xC2, 2, 0x0A, 0x00,
	0xC3, 2, 0x8A, 0x2A,
	0xC4, 2, 0x8A, 0xEE,
	0xC5, 1, 0x0E,
	0x20, 0,                        //inversion off
	0x36, 1, 0xC8,                  //MADCTL: MY | MX | BGR
	0x3A, 1, 0x05,                  //16 bit colour
	0xE0, 16, 0x02, 0x1c, 0x07, 0x12, 0x37, 0x32, 0x29, 0x2d, 0x29, 0x25, 0x2B, 0x39, 0x00, 0x01, 0x03, 0x10,
	0xE1, 16, 0x03, 0x1d, 0x07, 0x06, 0x2E, 0x2C, 0x29, 0x2D, 0x2E, 0x2E, 0x37, 0x3F, 0x00, 0x00, 0x02, 0x10,
	0x13, ST7735_DELAY, 10,         //normal display on
	0x29, ST7735_DELAY, 100,        //display on
};

static void RunCommands(const uint8_t* list)
{
	uint8_t count = *list++;
	SPI.beginTransaction(DISPLAY_SPI_SETTINGS);
	digitalWrite(PIN_LCD_CS, LOW);
	while (count--)
	{
		WriteCommand(*list++);
		uint8_t args = *list++;
		const bool delayAfter = (args & ST7735_DELAY) != 0;
		args &= ~ST7735_DELAY;
		while (args--)
			SpiSendByte(*list++);
		if (delayAfter)
			delay(*list++);
	}
	digitalWrite(PIN_LCD_CS, HIGH);
	SPI.endTransaction();
}

//the area of the display itself the pixels written next fill, both corners included
static void SetDisplayWindow(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1)
{
	ResetDriveHigh();
	WriteCommand(0x2A);
	SpiSendByte(0);
	SpiSendByte(x0);
	SpiSendByte(0);
	SpiSendByte(x1);
	WriteCommand(0x2B);
	SpiSendByte(0);
	SpiSendByte(y0);
	SpiSendByte(0);
	SpiSendByte(y1);
	WriteCommand(0x2C);
}

void PlatformCHGameDisplay::init(void)
{
	//the card shares the SPI and has to stay deselected while the panel is talked to
	pinMode(PIN_SD_CS, OUTPUT);
	digitalWrite(PIN_SD_CS, HIGH);
	pinMode(PIN_LCD_CS, OUTPUT);
	digitalWrite(PIN_LCD_CS, HIGH);
	pinMode(PIN_LCD_DC, OUTPUT);
	digitalWrite(PIN_LCD_DC, HIGH);
	pinMode(PIN_LCD_RST, OUTPUT);

	//a reset pulse, then the line stays high for as long as the game runs
	digitalWrite(PIN_LCD_RST, HIGH);
	delay(10);
	digitalWrite(PIN_LCD_RST, LOW);
	delay(10);
	digitalWrite(PIN_LCD_RST, HIGH);
	delay(120);

	SPI.begin();
	RunCommands(displayResetCommands);
	RunCommands(displayInitCommands);

	//whatever the panel powered up holding is cleared before the game draws
	startWrite();
	setAddrWindow(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
	writeColor(0x0000, (uint32_t)WINDOW_WIDTH * WINDOW_HEIGHT);
	endWrite();
}

void PlatformCHGameDisplay::startWrite(void)
{
	if (writeDepth++ == 0)
	{
		SPI.beginTransaction(DISPLAY_SPI_SETTINGS);
		digitalWrite(PIN_LCD_CS, LOW);
	}
}

void PlatformCHGameDisplay::endWrite(void)
{
	if (writeDepth && (--writeDepth == 0))
	{
		digitalWrite(PIN_LCD_CS, HIGH);
		SPI.endTransaction();
	}
}

void PlatformCHGameDisplay::setAddrWindow(int32_t x, int32_t y, int32_t w, int32_t h)
{
	if ((w <= 0) || (h <= 0))
		return;
	startWrite();
	SetDisplayWindow((uint8_t)(x + DISPLAY_OFFSET_X), (uint8_t)(y + DISPLAY_OFFSET_Y),
	                 (uint8_t)(x + DISPLAY_OFFSET_X + w - 1), (uint8_t)(y + DISPLAY_OFFSET_Y + h - 1));
	endWrite();
}

void PlatformCHGameDisplay::writePixels(const uint16_t* data, int32_t length, bool swap)
{
	startWrite();
	while (length > 0)
	{
		const int32_t run = (length > SPI_CHUNK_PIXELS) ? SPI_CHUNK_PIXELS : length;
		uint8_t* dst = spiChunk;
		if (swap)
		{
			for (int32_t i = 0; i < run; i++)
			{
				*dst++ = (uint8_t)(data[i] >> 8);
				*dst++ = (uint8_t)data[i];
			}
		}
		else
		{
			for (int32_t i = 0; i < run; i++)
			{
				*dst++ = (uint8_t)data[i];
				*dst++ = (uint8_t)(data[i] >> 8);
			}
		}
		SpiSend(spiChunk, (size_t)run * 2);
		data += run;
		length -= run;
	}
	endWrite();
}

void PlatformCHGameDisplay::writeColor(uint16_t color, uint32_t length)
{
	const uint8_t high = (uint8_t)(color >> 8), low = (uint8_t)color;
	startWrite();
	//the lump is filled once and sent as often as it takes
	uint32_t filled = (length > SPI_CHUNK_PIXELS) ? SPI_CHUNK_PIXELS : length;
	for (uint32_t i = 0; i < filled; i++)
	{
		spiChunk[i * 2] = high;
		spiChunk[i * 2 + 1] = low;
	}
	while (length)
	{
		const uint32_t run = (length > filled) ? filled : length;
		//the library may write over what it was given, so the lump is rebuilt when it does
		for (uint32_t i = 0; i < run; i++)
		{
			spiChunk[i * 2] = high;
			spiChunk[i * 2 + 1] = low;
		}
		SpiSend(spiChunk, (size_t)run * 2);
		length -= run;
	}
	endWrite();
}

void PlatformCHGameDisplay::writeBytes(const uint8_t* data, uint32_t length)
{
	startWrite();
	while (length)
	{
		const uint32_t run = (length > sizeof(spiChunk)) ? sizeof(spiChunk) : length;
		memcpy(spiChunk, data, run);
		SpiSend(spiChunk, run);
		data += run;
		length -= run;
	}
	endWrite();
}

//clips x, y, w, h to the game's screen, false when nothing of it is left
static bool ClipRect(int32_t& x, int32_t& y, int32_t& w, int32_t& h)
{
	if (x < 0) { w += x; x = 0; }
	if (y < 0) { h += y; y = 0; }
	if (x + w > WINDOW_WIDTH) w = WINDOW_WIDTH - x;
	if (y + h > WINDOW_HEIGHT) h = WINDOW_HEIGHT - y;
	return (w > 0) && (h > 0);
}

void PlatformCHGameDisplay::fillRect(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t color)
{
	if (!ClipRect(x, y, w, h))
		return;
	startWrite();
	setAddrWindow(x, y, w, h);
	writeColor(color, (uint32_t)w * h);
	endWrite();
}

void PlatformCHGameGFX::drawRect(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t color)
{
	if ((w <= 0) || (h <= 0))
		return;
	startWrite();
	fillRect(x, y, w, 1, color);
	fillRect(x, y + h - 1, w, 1, color);
	if (h > 2)
	{
		fillRect(x, y + 1, 1, h - 2, color);
		fillRect(x + w - 1, y + 1, 1, h - 2, color);
	}
	endWrite();
}

//Draws the character the way LovyanGFX draws its default font: a 6x8 cell times the text
//size, the 5 columns of the glyph and a column of background, the background only when it
//differs from the text colour. Every column goes out as runs of one colour
size_t PlatformCHGameGFX::drawChar(uint16_t c, int32_t x, int32_t y)
{
	const int32_t size = textSize;
	//the 'classic' character set LovyanGFX uses unless told otherwise
	if (c >= 176)
		c++;
	if (c > 255)
		return 6 * size;
	const bool fillBackground = (textBackground != textColor);
	const uint8_t* glyph = &platformFont[c * 5];
	startWrite();
	for (int32_t column = 0; column < 5; column++)
	{
		const uint8_t bits = glyph[column];
		int32_t row = 0;
		while (row < 8)
		{
			const bool set = ((bits >> row) & 1) != 0;
			int32_t end = row + 1;
			while ((end < 8) && ((((bits >> end) & 1) != 0) == set))
				end++;
			if (set || fillBackground)
				fillRect(x + column * size, y + row * size, size, (end - row) * size, set ? textColor : textBackground);
			row = end;
		}
	}
	if (fillBackground)
		fillRect(x + 5 * size, y, size, 8 * size, textBackground);
	endWrite();
	return 6 * size;
}

//Same character as the one above, but painted as one window: a run of pixels through fillRect
//costs a window of its own, and a window is command bytes the SPI has to drain before anything
//else can follow. A whole line of text was more of a frame than the board it was drawn over.
//A background the same as the text colour still goes the other way, there the pixels between the
//glyph are left as they are
#define FASTCHARSIZE 2   //text this size or smaller is built in the buffer below
//only taken from the heap the first time text is drawn this way, and kept from then on.
//NULL until then, or when that allocation failed, and the run path draws the character
static uint16_t* charCell = NULL;

size_t PlatformCHGameDisplay::drawChar(uint16_t c, int32_t x, int32_t y)
{
	const int32_t size = textSize;
	if (c >= 176)
		c++;
	if (c > 255)
		return 6 * size;
	const int32_t w = 6 * size, h = 8 * size;
	//a character that hangs off the screen is left to the clipping the run path does
	if ((textBackground == textColor) || (size > FASTCHARSIZE) ||
		(x < 0) || (y < 0) || (x + w > WINDOW_WIDTH) || (y + h > WINDOW_HEIGHT))
		return PlatformCHGameGFX::drawChar(c, x, y);

	if (!charCell)
	{
		charCell = (uint16_t*)malloc(6 * FASTCHARSIZE * 8 * FASTCHARSIZE * sizeof(uint16_t));
		//without it the character still goes out, a window per run of pixels
		if (!charCell)
			return PlatformCHGameGFX::drawChar(c, x, y);
	}

	//the cell is filled a glyph pixel at a time, never worked out per screen pixel
	const uint8_t* glyph = &platformFont[c * 5];
	uint16_t* d = charCell;
	for (int32_t glyphRow = 0; glyphRow < 8; glyphRow++)
	{
		const uint16_t* rowStart = d;
		for (int32_t glyphCol = 0; glyphCol < 5; glyphCol++)
		{
			const uint16_t color = (((glyph[glyphCol] >> glyphRow) & 1) != 0) ? textColor : textBackground;
			for (int32_t i = 0; i < size; i++)
				*d++ = color;
		}
		//the sixth column is the space between characters
		for (int32_t i = 0; i < size; i++)
			*d++ = textBackground;
		//a scaled cell repeats the row it just built
		for (int32_t i = 1; i < size; i++, d += w)
			memcpy(d, rowStart, w * sizeof(uint16_t));
	}
	startWrite();
	setAddrWindow(x, y, w, h);
	writePixels(charCell, w * h, true);
	endWrite();
	return 6 * size;
}

bool PlatformCHGameBuffer::createSprite(int32_t w, int32_t h)
{
	if (pixels)
		free(pixels);
	const size_t bytes = (size_t)((w + 7) / 8) * h;
	pixels = (uint8_t*)malloc(bytes);
	if (pixels)
		memset(pixels, 0, bytes);
	return pixels != nullptr;
}

void PlatformCHGameBuffer::fillRect(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t color)
{
	if (!pixels || !ClipRect(x, y, w, h))
		return;
#if SCREENBUFFER == 1
	(void)depth;
	for (int32_t row = y; row < y + h; row++)
		for (int32_t column = x; column < x + w; column++)
			SetBufferBit(pixels, column, row, color);
#else
	(void)depth;
	(void)color;
#endif
}

void Platform_Init(const char* appName)
{
	display.init();
	StorageInit(appName);

	//the buzzer is driven by tone(), which sets the pin up itself
	pinMode(PIN_BUZZER, OUTPUT);
	digitalWrite(PIN_BUZZER, LOW);

	//every button reads low while it is held
	pinMode(PIN_BTN_UP, INPUT_PULLUP);
	pinMode(PIN_BTN_DOWN, INPUT_PULLUP);
	pinMode(PIN_BTN_LEFT, INPUT_PULLUP);
	pinMode(PIN_BTN_RIGHT, INPUT_PULLUP);
	pinMode(PIN_BTN_A, INPUT_PULLUP);
	pinMode(PIN_BTN_B, INPUT_PULLUP);
	pinMode(PIN_BTN_SELECT, INPUT_PULLUP);
	pinMode(PIN_BTN_START, INPUT_PULLUP);
	//the pin changes above rewrite the port's configuration, see the note at the top
	ResetDriveHigh();

#if SCREENBUFFER
	screenBuffer.setColorDepth(SCREENBUFFER);
	if (!screenBuffer.createSprite(WINDOW_WIDTH, WINDOW_HEIGHT))
		Platform_Log("screen buffer could not be allocated\n");
#endif
	//last, the stack that is free from here on is what Platform_FreeStack measures
	PaintStack();
}

void Platform_SetBufferColors(uint16_t setColor, uint16_t clearColor)
{
#if SCREENBUFFER == 1
	bufferSetColor[0] = (uint8_t)(setColor >> 8);
	bufferSetColor[1] = (uint8_t)setColor;
	bufferClearColor[0] = (uint8_t)(clearColor >> 8);
	bufferClearColor[1] = (uint8_t)clearColor;
#else
	(void)setColor;
	(void)clearColor;
#endif
}

void Platform_PresentFrame(void)
{
#if SCREENBUFFER
	const uint8_t* src = (const uint8_t*)SCREENBUFFER_PIXELS();
	if (!src)
		return;
	//a row at a time, turned into the bytes the display takes
	uint8_t line[WINDOW_WIDTH * 2];
	display.startWrite();
	display.setAddrWindow(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
	for (int16_t y = 0; y < WINDOW_HEIGHT; y++)
	{
		uint8_t* dst = line;
		//a byte of the buffer at a time, most significant bit first as SetBufferBit writes
		for (int16_t x = 0; x < WINDOW_WIDTH; x += 8)
		{
			uint8_t bits = *src++;
			for (uint8_t b = 0; b < 8; b++, bits <<= 1)
			{
				const uint8_t* color = (bits & 0x80) ? bufferSetColor : bufferClearColor;
				*dst++ = color[0];
				*dst++ = color[1];
			}
		}
		display.writeBytes(line, sizeof(line));
	}
	display.endWrite();
#endif
}

// ===========================================================================
// Buttons
// ===========================================================================

uint8_t Platform_GetButtons(void)
{
	uint8_t buttons = 0;
	if (digitalRead(PIN_BTN_LEFT) == LOW)
		buttons |= BUTTON_LEFT;
	if (digitalRead(PIN_BTN_UP) == LOW)
		buttons |= BUTTON_UP;
	if (digitalRead(PIN_BTN_DOWN) == LOW)
		buttons |= BUTTON_DOWN;
	if (digitalRead(PIN_BTN_RIGHT) == LOW)
		buttons |= BUTTON_RIGHT;
	if (digitalRead(PIN_BTN_A) == LOW)
		buttons |= BUTTON_A;
	if (digitalRead(PIN_BTN_B) == LOW)
		buttons |= BUTTON_B;
	//the two side buttons, which the game uses to page through things
	if (digitalRead(PIN_BTN_SELECT) == LOW)
		buttons |= BUTTON_L;
	if (digitalRead(PIN_BTN_START) == LOW)
		buttons |= BUTTON_R;
	return buttons;
}

// ===========================================================================
// Sound
//
// The core drives the buzzer from TIM3, which carries the tone on its own once it is started:
// nothing here has to be counted per sample the way the Gamebuino's DAC is
// ===========================================================================

void Platform_PlayTone(uint16_t freq, uint16_t duration)
{
	//a frequency of 0 is a rest
	if (!freq)
	{
		noTone(PIN_BUZZER);
		return;
	}
	//tone() takes 0 as "until something stops it", which is what a duration of 0 means here too
	tone(PIN_BUZZER, freq, duration);
}

void Platform_StopTone(void)
{
	noTone(PIN_BUZZER);
}

// ===========================================================================
// Time and memory
// ===========================================================================

uint32_t Platform_Micros(void)
{
	return micros();
}

uint32_t Platform_Millis(void)
{
	return millis();
}

extern "C" char* sbrk(int increment);

//The heap grows up from the end of the program's data, the stack down from the end of RAM. Free
//heap is the gap between them plus what malloc has been given back
uint32_t Platform_FreeHeap(void)
{
	char* stackTop = (char*)__builtin_frame_address(0);
	return (uint32_t)(stackTop - sbrk(0)) + (uint32_t)mallinfo().fordblks;
}

#define STACK_PAINT 0xA5

//fills the gap between heap and stack with a pattern, what the stack reaches overwrites it
static void PaintStack(void)
{
	char* from = sbrk(0);
	//a little room below this function's own frame for the calls it makes
	char* to = (char*)__builtin_frame_address(0) - 64;
	for (char* p = from; p < to; p++)
		*p = (char)STACK_PAINT;
}

//the least stack that has been free since Platform_Init: the pattern left above the heap. Heap
//taken since then counts as used as well, heap and stack share the same gap
uint32_t Platform_FreeStack(void)
{
	char* p = sbrk(0);
	const char* limit = (char*)__builtin_frame_address(0);
	uint32_t count = 0;
	while ((p < limit) && (*p == (char)STACK_PAINT))
	{
		p++;
		count++;
	}
	return count;
}

uint32_t Platform_RandomSeed(void)
{
	//the time since power on hardly differs from one start to the next, the noise on an analog
	//input that nothing drives does
	return (micros() * micros()) ^ ((uint32_t)analogRead(PIN_A0) * analogRead(PIN_A1)) ^
	       ((uint32_t)analogRead(PIN_A2) << 16);
}

void Platform_Log(const char* format, ...)
{
	char text[128];
	va_list args;
	va_start(args, format);
	vsnprintf(text, sizeof(text), format, args);
	va_end(args);
	//Only to a computer that has the USB serial port open, it does not wait for one. This class
	//has no conversion to bool the way the other cores' do, dtr() is what says a host is there
	if (Serial.dtr())
		Serial.print(text);
}

// ===========================================================================
// Saved data
//
// The whole storage block is kept in RAM and in the file /<Game>_embedded/<Game>_embedded.sav on
// the microSD card, named after the first word of the game's name, the same as on the Gamebuino
// META. Bytes the file does not have yet read as 0xFF, the way erased flash does on the ESPboy,
// so the game sees a never saved store. Without a card the game still runs, it just does not keep
// anything.
//
// The chip's own EEPROM library is no use for this: on this part it stores into the option bytes
// and holds 26 of them, where the game's block is PLATFORM_STORAGE_SIZE.
// ===========================================================================

static uint8_t storage[PLATFORM_STORAGE_SIZE];

#ifdef CHGAME_HAS_SDFAT
static SdFat sd;
static bool sdReady = false;
static char storagePath[64] = "/game_embedded/game_embedded.sav";
#endif

static void StorageInit(const char* appName)
{
	memset(storage, 0xFF, sizeof(storage));
#ifdef CHGAME_HAS_SDFAT
	size_t n = 0;
	while (appName[n] && (appName[n] != ' ') && (n < 16))
		n++;
	if (n)
		snprintf(storagePath, sizeof(storagePath), "/%.*s_embedded/%.*s_embedded.sav",
		         (int)n, appName, (int)n, appName);

	sdReady = sd.begin(PIN_SD_CS, SD_SCK_MHZ(SD_SPI_HZ / 1000000));
	if (!sdReady)
	{
		Platform_Log("no SD card, nothing is saved\n");
		return;
	}
	File file = sd.open(storagePath, O_RDONLY);
	if (file)
	{
		file.read(storage, sizeof(storage));
		file.close();
	}
#else
	(void)appName;
	Platform_Log("built without SdFat, nothing is saved\n");
#endif
}

void Platform_StorageRead(uint16_t offset, uint8_t* data, uint16_t length)
{
	memcpy(data, storage + offset, length);
}

void Platform_StorageWrite(uint16_t offset, const uint8_t* data, uint16_t length)
{
	//storing what is already there does not touch the card
	if (memcmp(storage + offset, data, length) == 0)
		return;
	memcpy(storage + offset, data, length);
#ifdef CHGAME_HAS_SDFAT
	if (!sdReady)
		return;
	//the folder is the part of the path before the file name
	char folder[sizeof(storagePath)];
	strcpy(folder, storagePath);
	char* slash = strrchr(folder, '/');
	if (slash && (slash != folder))
	{
		*slash = '\0';
		if (!sd.exists(folder))
			sd.mkdir(folder);
	}
	File file = sd.open(storagePath, O_WRONLY | O_CREAT | O_TRUNC);
	if (!file)
	{
		Platform_Log("could not write %s\n", storagePath);
		return;
	}
	file.write(storage, sizeof(storage));
	file.close();
#endif
}

#endif
