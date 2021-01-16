/*-----------------------------------------------------------/
/ LcdCanvas
/------------------------------------------------------------/
/ Copyright (c) 2020, Elehobica
/ Released under the BSD-2-Clause
/ refer to https://opensource.org/licenses/BSD-2-Clause
/-----------------------------------------------------------*/

#ifndef __LCDCANVAS_H_INCLUDED__
#define __LCDCANVAS_H_INCLUDED__

#include <LcdElementBox.h>

// Select LCD device
//#define USE_ST7735_128x160
#define USE_ST7789_240x240_WOCS
//#define USE_ILI9341_240x320

#ifdef USE_ST7735_128x160
#include <Adafruit_ST7735.h> // Hardware-specific library for ST7735
#define Adafruit_LCD Adafruit_ST7735
#define CUSTOM_FONT	(&Nimbus_Sans_L_Regular_Condensed_12)
#define CUSTOM_FONT_OFS_Y	13
#endif
#ifdef USE_ST7789_240x240_WOCS
#include <Adafruit_ST7789.h> // Hardware-specific library for ST7789
#define Adafruit_LCD Adafruit_ST7789
#define CUSTOM_FONT	(&Nimbus_Sans_L_Regular_Condensed_16)
#define CUSTOM_FONT_OFS_Y	13
#define USE_ALBUM_ART_SMALL
#endif
#ifdef USE_ILI9341_240x320
#include <Adafruit_ILI9341.h> // Hardware-specific library for ILI9341
#define Adafruit_LCD Adafruit_ILI9341
#define CUSTOM_FONT	(&Nimbus_Sans_L_Regular_Condensed_12)
#define CUSTOM_FONT_OFS_Y	13
#endif

//#include <Fonts/FreeMono9pt7b.h>
// Custom Font (gfxFont)
#include "fonts/Nimbus_Sans_L_Regular_Condensed_12.h"
#include "fonts/Nimbus_Sans_L_Regular_Condensed_16.h"
//#include "fonts/Open_Sans_Condensed_Light_12.h"
//#include "fonts/Open_Sans_Condensed_Light_16.h"

#include "fonts/iconfont.h"

//#define DEBUG_LCD_CANVAS

// Common for Unicode Font & Custom Font (gfxFont)
#define FONT_HEIGHT		16

//extern uint8_t Icon16[];
#define ICON16x16_TITLE		&Icon16[32*0]
#define ICON16x16_ARTIST	&Icon16[32*1]
#define ICON16x16_ALBUM		&Icon16[32*2]
#define ICON16x16_FOLDER	&Icon16[32*3]
#define ICON16x16_FILE		&Icon16[32*4]
#define ICON16x16_VOLUME	&Icon16[32*5]
#define ICON16x16_BATTERY	&Icon16[32*6]
#define ICON16x16_YEAR		&Icon16[32*7]
#define ICON16x16_GEAR		&Icon16[32*8]
#define ICON16x16_CHECKED	&Icon16[32*9]
#define ICON16x16_LEFTARROW	&Icon16[32*10]

//=================================
// Definition of BatteryIconBox class < IconBox
//=================================
class BatteryIconBox : public IconBox
{
public:
	BatteryIconBox(int16_t pos_x, int16_t pos_y, uint16_t fgColor = LCD_WHITE, uint16_t bgColor = LCD_BLACK);
	void draw(Adafruit_SPITFT *tft);
	void setLevel(uint8_t value);
protected:
	uint8_t level;
};

//=================================
// Definition of LcdCanvas Class < Adafruit_LCD
//=================================
class LcdCanvas : public Adafruit_LCD
{
public:
	//LcdCanvas(int8_t cs, int8_t dc, int8_t mosi, int8_t sclk, int8_t rst) : Adaruit_LCD(cs, dc, mosi, sclk, rst) {}
	LcdCanvas(int8_t cs, int8_t dc, int8_t rst);
#if !defined(ESP8266)
	//LcdCanvas(SPIClass *spiClass, int8_t cs, int8_t dc, int8_t rst) : Adaruit_LCD(spiClass, cs, dc, rst) {}
#endif // end !ESP8266

    ~LcdCanvas();
	void clear();
	void bye();
	void setListItem(int column, const char *str, const uint8_t *icon = NULL, bool isFocused = false, encoding_t = none);
	void setBitRate(uint16_t value);
	void setVolume(uint8_t value);
	void setPlayTime(uint32_t posionSec, uint32_t lengthSec, bool blink = false);
	void setTrack(const char *str);
	void setTitle(const char *str, encoding_t encoding = none);
	void setAlbum(const char *str, encoding_t encoding = none);
	void setArtist(const char *str, encoding_t encoding = none);
	void setYear(const char *str, encoding_t encoding = none);
	void setBatteryVoltage(uint16_t voltage_x1000);
	void addAlbumArtJpeg(uint16_t file_idx, uint64_t pos, size_t size, bool is_unsync = false);
	void addAlbumArtPng(uint16_t file_idx, uint64_t pos, size_t size, bool is_unsync = false);
	void deleteAlbumArt();
	void switchToListView();
	void switchToPlay();
	void switchToPowerOff(const char *msg_str = NULL);
	void drawListView();
	void drawPlay();
	void drawPowerOff();

protected:
	int play_count;
	const int play_cycle = 400;
	const int play_change = 350;
	#ifdef USE_ST7735_128x160
	IconScrollTextBox listItem[10] = {
		IconScrollTextBox(16*0, 16*0, width(), LCD_GRAY, LCD_BLACK, CUSTOM_FONT, CUSTOM_FONT_OFS_Y, FONT_HEIGHT),
		IconScrollTextBox(16*0, 16*1, width(), LCD_GRAY, LCD_BLACK, CUSTOM_FONT, CUSTOM_FONT_OFS_Y, FONT_HEIGHT),
		IconScrollTextBox(16*0, 16*2, width(), LCD_GRAY, LCD_BLACK, CUSTOM_FONT, CUSTOM_FONT_OFS_Y, FONT_HEIGHT),
		IconScrollTextBox(16*0, 16*3, width(), LCD_GRAY, LCD_BLACK, CUSTOM_FONT, CUSTOM_FONT_OFS_Y, FONT_HEIGHT),
		IconScrollTextBox(16*0, 16*4, width(), LCD_GRAY, LCD_BLACK, CUSTOM_FONT, CUSTOM_FONT_OFS_Y, FONT_HEIGHT),
		IconScrollTextBox(16*0, 16*5, width(), LCD_GRAY, LCD_BLACK, CUSTOM_FONT, CUSTOM_FONT_OFS_Y, FONT_HEIGHT),
		IconScrollTextBox(16*0, 16*6, width(), LCD_GRAY, LCD_BLACK, CUSTOM_FONT, CUSTOM_FONT_OFS_Y, FONT_HEIGHT),
		IconScrollTextBox(16*0, 16*7, width(), LCD_GRAY, LCD_BLACK, CUSTOM_FONT, CUSTOM_FONT_OFS_Y, FONT_HEIGHT),
		IconScrollTextBox(16*0, 16*8, width(), LCD_GRAY, LCD_BLACK, CUSTOM_FONT, CUSTOM_FONT_OFS_Y, FONT_HEIGHT),
		IconScrollTextBox(16*0, 16*9, width(), LCD_GRAY, LCD_BLACK, CUSTOM_FONT, CUSTOM_FONT_OFS_Y, FONT_HEIGHT)
	};
	BatteryIconBox battery = BatteryIconBox(width()-16, 16*0, LCD_GRAY);
	IconTextBox volume = IconTextBox(16*0, 16*0, ICON16x16_VOLUME, LCD_GRAY);
	TextBox bitRate = TextBox(width()/2, 16*0, LcdElementBox::AlignCenter, LCD_GRAY);
	NFTextBox playTime = NFTextBox(width(), height()-16, width(), LcdElementBox::AlignRight, LCD_GRAY, LCD_BLACK, CUSTOM_FONT, CUSTOM_FONT_OFS_Y, FONT_HEIGHT);
	IconScrollTextBox title = IconScrollTextBox(16*0, 16*3, ICON16x16_TITLE, width(), LCD_WHITE, LCD_BLACK, CUSTOM_FONT, CUSTOM_FONT_OFS_Y, FONT_HEIGHT);
	IconScrollTextBox artist = IconScrollTextBox(16*0, 16*4, ICON16x16_ARTIST, width(), LCD_WHITE, LCD_BLACK, CUSTOM_FONT, CUSTOM_FONT_OFS_Y, FONT_HEIGHT);
	IconScrollTextBox album = IconScrollTextBox(16*0, 16*5, ICON16x16_ALBUM, width(), LCD_WHITE, LCD_BLACK, CUSTOM_FONT, CUSTOM_FONT_OFS_Y, FONT_HEIGHT);
	IconTextBox year = IconTextBox(16*0, 16*6, ICON16x16_YEAR);
	TextBox track = TextBox(16*0, height()-16, LcdElementBox::AlignLeft, LCD_GRAY);
	TextBox msg = TextBox(width()/2, height()/2-FONT_HEIGHT, LcdElementBox::AlignCenter);
	ImageBox albumArt = ImageBox(0, (height() - width())/2, width(), width());
	LcdElementBox *groupListView[10] = {
		&listItem[0], &listItem[1], &listItem[2], &listItem[3], &listItem[4], &listItem[5], &listItem[6], &listItem[7], &listItem[8], &listItem[9]
	};
	LcdElementBox *groupPlay[5] = {&battery, &volume, &bitRate, &playTime, &track}; // Common for Play mode 0 and 1
	LcdElementBox *groupPlay0[4] = {&title, &artist, &album, &year}; // Play mode 0 only
	LcdElementBox *groupPlay1[2] = {&albumArt, &msg}; // Play mode 1 only
	LcdElementBox *groupPowerOff[1] = {&msg};
	#endif // USE_ST7735_128x160
	#ifdef USE_ST7789_240x240_WOCS
	IconScrollTextBox listItem[15] = {
		IconScrollTextBox(16*0, 16*0, width(), LCD_GRAY, LCD_BLACK, CUSTOM_FONT, CUSTOM_FONT_OFS_Y, FONT_HEIGHT),
		IconScrollTextBox(16*0, 16*1, width(), LCD_GRAY, LCD_BLACK, CUSTOM_FONT, CUSTOM_FONT_OFS_Y, FONT_HEIGHT),
		IconScrollTextBox(16*0, 16*2, width(), LCD_GRAY, LCD_BLACK, CUSTOM_FONT, CUSTOM_FONT_OFS_Y, FONT_HEIGHT),
		IconScrollTextBox(16*0, 16*3, width(), LCD_GRAY, LCD_BLACK, CUSTOM_FONT, CUSTOM_FONT_OFS_Y, FONT_HEIGHT),
		IconScrollTextBox(16*0, 16*4, width(), LCD_GRAY, LCD_BLACK, CUSTOM_FONT, CUSTOM_FONT_OFS_Y, FONT_HEIGHT),
		IconScrollTextBox(16*0, 16*5, width(), LCD_GRAY, LCD_BLACK, CUSTOM_FONT, CUSTOM_FONT_OFS_Y, FONT_HEIGHT),
		IconScrollTextBox(16*0, 16*6, width(), LCD_GRAY, LCD_BLACK, CUSTOM_FONT, CUSTOM_FONT_OFS_Y, FONT_HEIGHT),
		IconScrollTextBox(16*0, 16*7, width(), LCD_GRAY, LCD_BLACK, CUSTOM_FONT, CUSTOM_FONT_OFS_Y, FONT_HEIGHT),
		IconScrollTextBox(16*0, 16*8, width(), LCD_GRAY, LCD_BLACK, CUSTOM_FONT, CUSTOM_FONT_OFS_Y, FONT_HEIGHT),
		IconScrollTextBox(16*0, 16*9, width(), LCD_GRAY, LCD_BLACK, CUSTOM_FONT, CUSTOM_FONT_OFS_Y, FONT_HEIGHT),
		IconScrollTextBox(16*0, 16*10, width(), LCD_GRAY, LCD_BLACK, CUSTOM_FONT, CUSTOM_FONT_OFS_Y, FONT_HEIGHT),
		IconScrollTextBox(16*0, 16*11, width(), LCD_GRAY, LCD_BLACK, CUSTOM_FONT, CUSTOM_FONT_OFS_Y, FONT_HEIGHT),
		IconScrollTextBox(16*0, 16*12, width(), LCD_GRAY, LCD_BLACK, CUSTOM_FONT, CUSTOM_FONT_OFS_Y, FONT_HEIGHT),
		IconScrollTextBox(16*0, 16*13, width(), LCD_GRAY, LCD_BLACK, CUSTOM_FONT, CUSTOM_FONT_OFS_Y, FONT_HEIGHT),
		IconScrollTextBox(16*0, 16*14, width(), LCD_GRAY, LCD_BLACK, CUSTOM_FONT, CUSTOM_FONT_OFS_Y, FONT_HEIGHT)
	};
	BatteryIconBox battery = BatteryIconBox(width()-16, 16*0, LCD_GRAY);
	IconTextBox volume = IconTextBox(16*0, 16*0, ICON16x16_VOLUME, LCD_GRAY);
	TextBox bitRate = TextBox(width()/2, 16*0, LcdElementBox::AlignCenter, LCD_GRAY);
	NFTextBox playTime = NFTextBox(width(), 240-16, width(), LcdElementBox::AlignRight, LCD_GRAY, LCD_BLACK, CUSTOM_FONT, CUSTOM_FONT_OFS_Y, FONT_HEIGHT);
	IconScrollTextBox title = IconScrollTextBox(16*0, 16*10, ICON16x16_TITLE, width(), LCD_WHITE, LCD_BLACK, CUSTOM_FONT, CUSTOM_FONT_OFS_Y, FONT_HEIGHT);
	IconScrollTextBox artist = IconScrollTextBox(16*0, 16*11, ICON16x16_ARTIST, width(), LCD_WHITE, LCD_BLACK, CUSTOM_FONT, CUSTOM_FONT_OFS_Y, FONT_HEIGHT);
	IconScrollTextBox album = IconScrollTextBox(16*0, 16*12, ICON16x16_ALBUM, width(), LCD_WHITE, LCD_BLACK, CUSTOM_FONT, CUSTOM_FONT_OFS_Y, FONT_HEIGHT);
	IconTextBox year = IconTextBox(16*0, 16*13, ICON16x16_YEAR);
	TextBox track = TextBox(0, 240-16, LcdElementBox::AlignLeft, LCD_GRAY);
	TextBox msg = TextBox(width()/2, 240/2-FONT_HEIGHT, LcdElementBox::AlignCenter);
	ImageBox albumArtSmall = ImageBox((240-16*9)/2, 16*1, 16*9, 16*9);
	ImageBox albumArt = ImageBox(0, (240 - width())/2, width(), width());
	LcdElementBox *groupListView[15] = {
		&listItem[0],  &listItem[1],  &listItem[2],  &listItem[3],  &listItem[4], &listItem[5], &listItem[6], &listItem[7], &listItem[8], &listItem[9],
		&listItem[10], &listItem[11], &listItem[12], &listItem[13], &listItem[14]
	};
	LcdElementBox *groupPlay[1] = {&msg}; // Common for Play mode 0 and 1
	LcdElementBox *groupPlay0[10] = {&battery, &volume, &bitRate, &playTime, &track, &title, &artist, &album, &year, &albumArtSmall}; // Play mode 0 only
	LcdElementBox *groupPlay1[1] = {&albumArt}; // Play mode 1 only
	LcdElementBox *groupPowerOff[1] = {&msg};
	#endif // USE_ST7789_240x240_WOCS
	#ifdef USE_ILI9341_240x320
	IconScrollTextBox listItem[20] = {
		IconScrollTextBox(16*0, 16*0, width(), LCD_GRAY, LCD_BLACK, CUSTOM_FONT, CUSTOM_FONT_OFS_Y, FONT_HEIGHT),
		IconScrollTextBox(16*0, 16*1, width(), LCD_GRAY, LCD_BLACK, CUSTOM_FONT, CUSTOM_FONT_OFS_Y, FONT_HEIGHT),
		IconScrollTextBox(16*0, 16*2, width(), LCD_GRAY, LCD_BLACK, CUSTOM_FONT, CUSTOM_FONT_OFS_Y, FONT_HEIGHT),
		IconScrollTextBox(16*0, 16*3, width(), LCD_GRAY, LCD_BLACK, CUSTOM_FONT, CUSTOM_FONT_OFS_Y, FONT_HEIGHT),
		IconScrollTextBox(16*0, 16*4, width(), LCD_GRAY, LCD_BLACK, CUSTOM_FONT, CUSTOM_FONT_OFS_Y, FONT_HEIGHT),
		IconScrollTextBox(16*0, 16*5, width(), LCD_GRAY, LCD_BLACK, CUSTOM_FONT, CUSTOM_FONT_OFS_Y, FONT_HEIGHT),
		IconScrollTextBox(16*0, 16*6, width(), LCD_GRAY, LCD_BLACK, CUSTOM_FONT, CUSTOM_FONT_OFS_Y, FONT_HEIGHT),
		IconScrollTextBox(16*0, 16*7, width(), LCD_GRAY, LCD_BLACK, CUSTOM_FONT, CUSTOM_FONT_OFS_Y, FONT_HEIGHT),
		IconScrollTextBox(16*0, 16*8, width(), LCD_GRAY, LCD_BLACK, CUSTOM_FONT, CUSTOM_FONT_OFS_Y, FONT_HEIGHT),
		IconScrollTextBox(16*0, 16*9, width(), LCD_GRAY, LCD_BLACK, CUSTOM_FONT, CUSTOM_FONT_OFS_Y, FONT_HEIGHT),
		IconScrollTextBox(16*0, 16*10, width(), LCD_GRAY, LCD_BLACK, CUSTOM_FONT, CUSTOM_FONT_OFS_Y, FONT_HEIGHT),
		IconScrollTextBox(16*0, 16*11, width(), LCD_GRAY, LCD_BLACK, CUSTOM_FONT, CUSTOM_FONT_OFS_Y, FONT_HEIGHT),
		IconScrollTextBox(16*0, 16*12, width(), LCD_GRAY, LCD_BLACK, CUSTOM_FONT, CUSTOM_FONT_OFS_Y, FONT_HEIGHT),
		IconScrollTextBox(16*0, 16*13, width(), LCD_GRAY, LCD_BLACK, CUSTOM_FONT, CUSTOM_FONT_OFS_Y, FONT_HEIGHT),
		IconScrollTextBox(16*0, 16*14, width(), LCD_GRAY, LCD_BLACK, CUSTOM_FONT, CUSTOM_FONT_OFS_Y, FONT_HEIGHT),
		IconScrollTextBox(16*0, 16*15, width(), LCD_GRAY, LCD_BLACK, CUSTOM_FONT, CUSTOM_FONT_OFS_Y, FONT_HEIGHT),
		IconScrollTextBox(16*0, 16*16, width(), LCD_GRAY, LCD_BLACK, CUSTOM_FONT, CUSTOM_FONT_OFS_Y, FONT_HEIGHT),
		IconScrollTextBox(16*0, 16*17, width(), LCD_GRAY, LCD_BLACK, CUSTOM_FONT, CUSTOM_FONT_OFS_Y, FONT_HEIGHT),
		IconScrollTextBox(16*0, 16*18, width(), LCD_GRAY, LCD_BLACK, CUSTOM_FONT, CUSTOM_FONT_OFS_Y, FONT_HEIGHT),
		IconScrollTextBox(16*0, 16*19, width(), LCD_GRAY, LCD_BLACK, CUSTOM_FONT, CUSTOM_FONT_OFS_Y, FONT_HEIGHT)
	};
	BatteryIconBox battery = BatteryIconBox(width()-16, 16*0, LCD_GRAY);
	IconTextBox volume = IconTextBox(16*0, 16*0, ICON16x16_VOLUME, LCD_GRAY);
	TextBox bitRate = TextBox(width()/2, 16*0, LcdElementBox::AlignCenter, LCD_GRAY);
	NFTextBox playTime = NFTextBox(width(), height()-16, width(), LcdElementBox::AlignRight, LCD_GRAY, LCD_BLACK, CUSTOM_FONT, CUSTOM_FONT_OFS_Y, FONT_HEIGHT);
	IconScrollTextBox title = IconScrollTextBox(16*0, 16*16, ICON16x16_TITLE, width(), LCD_WHITE, LCD_BLACK, CUSTOM_FONT, CUSTOM_FONT_OFS_Y, FONT_HEIGHT);
	IconScrollTextBox artist = IconScrollTextBox(16*0, 16*17, ICON16x16_ARTIST, width(), LCD_WHITE, LCD_BLACK, CUSTOM_FONT, CUSTOM_FONT_OFS_Y, FONT_HEIGHT);
	IconScrollTextBox album = IconScrollTextBox(16*0, 16*18, ICON16x16_ALBUM, width(), LCD_WHITE, LCD_BLACK, CUSTOM_FONT, CUSTOM_FONT_OFS_Y, FONT_HEIGHT);
	IconTextBox year = IconTextBox(16*0, 16*19, ICON16x16_YEAR);
	TextBox track = TextBox(width()/2, height()-16, LcdElementBox::AlignCenter, LCD_GRAY);
	TextBox msg = TextBox(width()/2, height()/2-FONT_HEIGHT, LcdElementBox::AlignCenter);
	ImageBox albumArt = ImageBox(0, 16*1, width(), width());
	LcdElementBox *groupListView[20] = {
		&listItem[0],  &listItem[1],  &listItem[2],  &listItem[3],  &listItem[4],  &listItem[5],  &listItem[6],  &listItem[7],  &listItem[8],  &listItem[9],
		&listItem[10], &listItem[11], &listItem[12], &listItem[13], &listItem[14], &listItem[15], &listItem[16], &listItem[17], &listItem[18], &listItem[19]
	};
	LcdElementBox *groupPlay[11] = {&battery, &volume, &bitRate, &playTime, &track, &title, &artist, &album, &year, &albumArt, &msg}; // Common for Play mode 0 and 1
	LcdElementBox *groupPlay0[0] = {}; // Play mode 0 only
	LcdElementBox *groupPlay1[0] = {}; // Play mode 1 only
	LcdElementBox *groupPowerOff[1] = {&msg};
	#endif // USE_ILI9341_240x320
};

#endif // __LCDCANVAS_H_INCLUDED__
