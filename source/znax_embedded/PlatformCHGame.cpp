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
//for SPI1's own registers, which the pixels go out through, see SpiSend below
extern "C" {
#include "ch32x035.h"
}
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
//The eight pixels of every byte a 1 bpp buffer can hold, ready for the display. Working them out
//a pixel at a time cost 18 of the 44 ms a frame took, as much as the whole transfer; this way a
//row is sixteen copies of sixteen bytes. It is 4 KB of the 20, and is built again whenever the
//two colours change
static uint8_t bufferExpand[256][16];

static void BuildExpandTable(void)
{
	for (int i = 0; i < 256; i++)
	{
		for (int b = 0; b < 8; b++)
		{
			const uint8_t* color = (i & (0x80 >> b)) ? bufferSetColor : bufferClearColor;
			bufferExpand[i][b * 2] = color[0];
			bufferExpand[i][b * 2 + 1] = color[1];
		}
	}
}
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

//Bytes go out through SPI1's own register rather than through the library. Its transfer waits on
//the flag through a function call and asks the tick counter twice a byte to see whether it has
//timed out, which is around a hundred thousand calls for a frame of 32 KB: the game ran at 8
//frames a second with them and the wire itself only takes 11 ms of the 33 a frame has. The panel
//is only ever written to, so nothing is read back and the receive buffer is left to overrun.
//SPI.beginTransaction still sets the clock and the mode, only the data goes this way
static inline void SpiSend(const uint8_t* data, size_t length)
{
	while (length--)
	{
		while (!(SPI1->STATR & SPI_STATR_TXE))
			;
		SPI1->DATAR = *data++;
	}
}

static inline void SpiSendByte(uint8_t value)
{
	while (!(SPI1->STATR & SPI_STATR_TXE))
		;
	SPI1->DATAR = value;
}

//waits for the last byte to leave, before the data/command line or the chip select moves
static inline void SpiWait(void)
{
	while (SPI1->STATR & SPI_STATR_BSY)
		;
}

// ---------------------------------------------------------------------------
// The pixels go out by DMA
//
// Feeding the register by hand only keeps the wire half busy: the clock really is 24 MHz, but the
// loop cannot fetch, test and store fast enough out of flash to hand it a byte every 16 cycles, so
// a frame took 22 ms where the bytes themselves are 11. DMA1 channel 3 is SPI1's transmit request,
// and once it is started the frame costs what its bytes cost and the processor is free to build
// the next row meanwhile. This is what CHGfx does, and why it reaches the wire's own limit
// ---------------------------------------------------------------------------

//Memory to peripheral, a byte at a time, the memory address stepping and the peripheral's standing
//still, at the highest priority. The names are the chip header's own: leaving MINC out sends the
//first byte over and over, and channel 3's finished flag is 0x200 where 0x20 is channel 2's
#define DMA_CFGR_TO_SPI (DMA_CFGR1_DIR | DMA_CFGR1_MINC | DMA_CFGR1_PL)
#define DMA_CFGR_ENABLE DMA_CFGR1_EN
#define DMA_FLAG_TC3 DMA1_FLAG_TC3
//all four of channel 3's flags, cleared together as CHGfx does
#define DMA_FLAGS_CH3 ((uint32_t)0x00000F00)

static bool dmaBusy = false;

static void DmaInit(void)
{
	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);
	DMA1_Channel3->CFGR = 0;
	DMA1_Channel3->PADDR = (uint32_t)&SPI1->DATAR;
	SPI1->CTLR2 |= SPI_I2S_DMAReq_Tx;
}

//Set once the DMA has been seen not to work, after which the rows go out by hand instead. A frame
//that never finishes would leave the screen black and nothing to read, which is exactly what a
//wrong channel or a wrong flag did while this was being written
static bool dmaBroken = false;

//hands the bytes over and returns at once, they are on their way while this goes on
static void DmaSend(const uint8_t* data, uint16_t length)
{
	if (dmaBroken)
	{
		SpiSend(data, length);
		return;
	}
	//The order is CHGfx's, which is the one that works: the transmit request is turned off while
	//the channel is set up, the settings are written without the enable bit, the request is turned
	//on, and only then is the channel started. SPI.beginTransaction also puts CTLR2 back as it was
	//for every transaction, so the request is asked for here and not once at the start
	SPI1->CTLR2 &= (uint16_t)~SPI_I2S_DMAReq_Tx;
	DMA1_Channel3->CFGR = 0;
	DMA1->INTFCR = DMA_FLAGS_CH3;
	DMA1_Channel3->MADDR = (uint32_t)data;
	DMA1_Channel3->CNTR = length;
	DMA1_Channel3->CFGR = DMA_CFGR_TO_SPI;
	SPI1->CTLR2 |= SPI_I2S_DMAReq_Tx;
	DMA1_Channel3->CFGR |= DMA_CFGR_ENABLE;
	dmaBusy = true;
}

static void DmaWait(void)
{
	if (!dmaBusy)
		return;
	//A row is 256 bytes, which at 24 MHz is about 85 us. This waits many times longer than that
	//and then gives up rather than standing here for good
	uint32_t spins = 2000000;
	while (!(DMA1->INTFR & DMA_FLAG_TC3))
	{
		if (--spins == 0)
		{
			dmaBroken = true;
			break;
		}
	}
	DMA1_Channel3->CFGR = 0;
	DMA1->INTFCR = DMA_FLAGS_CH3;
	SPI1->CTLR2 &= (uint16_t)~SPI_I2S_DMAReq_Tx;
	dmaBusy = false;
	//the last byte sits in the shift register for a while after the DMA has done with it
	SpiWait();
}

//see the note at the top: the panel takes a drifting reset line for a real reset
static inline void ResetDriveHigh(void)
{
	digitalWrite(PIN_LCD_RST, HIGH);
}

//The data/command line only means anything while the byte it belongs to is on the wire, so it is
//moved with the wire idle on both sides of the command
static void WriteCommand(uint8_t command)
{
	SpiWait();
	digitalWrite(PIN_LCD_DC, LOW);
	SpiSendByte(command);
	SpiWait();
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
		{
			SpiWait();
			delay(*list++);
		}
	}
	SpiWait();
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
		//the chip select only goes up once the last byte has really left
		SpiWait();
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
		SpiSend(spiChunk, (size_t)run * 2);
		length -= run;
	}
	endWrite();
}

void PlatformCHGameDisplay::writeBytes(const uint8_t* data, uint32_t length)
{
	//nothing is written over what it is given any more, so the caller's bytes go out where they lie
	startWrite();
	SpiSend(data, length);
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
	//after the display, so the SPI is already up when its transmit request is turned on
	DmaInit();
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
	//black and white until the game says otherwise, so the table is never read unbuilt
	BuildExpandTable();
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
	//the table holds these two colours, so it is built again whenever they change
	BuildExpandTable();
#else
	(void)setColor;
	(void)clearColor;
#endif
}

//Temporary: says over the USB serial where a frame's time goes, so the part worth working on is
//known rather than guessed. Build with -DCHGAME_TIMING=1 and -DFPSLOCK=0, or the frame lock hides
//the answer by waiting out whatever is left of the 33 ms
#ifndef CHGAME_TIMING
#define CHGAME_TIMING 0
#endif

void Platform_PresentFrame(void)
{
#if CHGAME_TIMING
	static uint32_t sendSum = 0, buildSum = 0, frameSum = 0, lastStart = 0;
	static uint16_t counted = 0;
	const uint32_t tStart = micros();
	uint32_t buildUs = 0;
#endif
#if SCREENBUFFER
	const uint8_t* src = (const uint8_t*)SCREENBUFFER_PIXELS();
	if (!src)
		return;
	//Two rows, so the one being built is never the one on the wire. The row is turned into the
	//display's bytes while the row before it is still going out, which is what keeps the wire from
	//ever standing idle
	static uint8_t lines[2][WINDOW_WIDTH * 2];
	uint8_t which = 0;
	display.startWrite();
	display.setAddrWindow(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
	for (int16_t y = 0; y < WINDOW_HEIGHT; y++)
	{
		uint8_t* line = lines[which];
		which ^= 1;
#if CHGAME_TIMING
		const uint32_t tRow = micros();
#endif
		uint8_t* dst = line;
		//a byte of the buffer is eight pixels the table already holds
		for (int16_t x = 0; x < WINDOW_WIDTH; x += 8, dst += 16)
			memcpy(dst, bufferExpand[*src++], 16);
#if CHGAME_TIMING
		buildUs += micros() - tRow;
#endif
		//the row before this one is given until now to finish, then this one goes on its way and
		//the next is built while it travels
		DmaWait();
		DmaSend(line, WINDOW_WIDTH * 2);
	}
	DmaWait();
	display.endWrite();
#endif
#if CHGAME_TIMING
	const uint32_t tEnd = micros();
	sendSum += (tEnd - tStart) - buildUs;
	buildSum += buildUs;
	if (lastStart)
		frameSum += tStart - lastStart;
	lastStart = tStart;
	if (++counted >= 30)
	{
		//the frame is everything between one present and the next: the game's own drawing is
		//whatever is left once the two below are taken off it
		Platform_Log("frame %6lu us  build %5lu  send %5lu  game %6lu  heap %5lu  stack %4lu%s\n",
		             (unsigned long)(frameSum / counted), (unsigned long)(buildSum / counted),
		             (unsigned long)(sendSum / counted),
		             (unsigned long)((frameSum - buildSum - sendSum) / counted),
		             (unsigned long)Platform_FreeHeap(), (unsigned long)Platform_FreeStack(),
		             dmaBroken ? "  (DMA gave up, rows go out by hand)" : "");
		sendSum = buildSum = frameSum = 0;
		counted = 0;
	}
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

//Where the linker put things, from link_chgame_app.ld. This chip does not lay its RAM out the way
//the Gamebuino does: there the heap grows up and the stack down into one shared gap, here the
//stack is a region of its own of __stack_size bytes at the very top and the heap has a ceiling of
//its own below it. So the two are measured apart, and neither can be worked out from the other
extern "C" {
extern char _end[];         //the end of the program's data, where the heap starts
extern char _heap_end[];    //as far up as the heap may ever grow
extern char _susrstack[];   //the bottom of the stack, which is as far down as it may grow
extern char _eusrstack[];   //the top of RAM, where the stack starts and grows downwards
}

//What is left for the heap: the part of its room it has not asked for yet, and what it has asked
//for but handed back. sbrk is not used, the C library here does not have to offer one
uint32_t Platform_FreeHeap(void)
{
	const struct mallinfo info = mallinfo();
	const uint32_t room = (uint32_t)(_heap_end - _end);
	const uint32_t taken = (uint32_t)info.arena;
	return (taken > room ? 0 : room - taken) + (uint32_t)info.fordblks;
}

#define STACK_PAINT 0xA5

//Fills the stack below what is in use with a pattern. What the stack reaches later writes over it,
//so what is left of it says how close the stack has ever come to the bottom
static void PaintStack(void)
{
	//everything under this function's own frame, with a little room for the calls it makes
	char* to = (char*)__builtin_frame_address(0) - 64;
	if (to > _eusrstack)
		to = _eusrstack;
	for (char* p = _susrstack; p < to; p++)
		*p = (char)STACK_PAINT;
}

//the least stack that has been free since Platform_Init: the pattern still standing at the bottom
uint32_t Platform_FreeStack(void)
{
	const char* p = _susrstack;
	uint32_t count = 0;
	while ((p < _eusrstack) && (*p == (char)STACK_PAINT))
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
