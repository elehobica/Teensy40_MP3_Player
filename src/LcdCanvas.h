#ifndef __LCDCANVAS_H_INCLUDED__
#define __LCDCANVAS_H_INCLUDED__

#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7735.h> // Hardware-specific library for ST7735
//#include <Fonts/FreeMono9pt7b.h>
#include "Nimbus_Sans_L_Regular_Condensed_12.h"

#define DEFAULT_FONT	(&Nimbus_Sans_L_Regular_Condensed_12)
#define FONT_HEIGHT		16

// Additional Colors for ST77XX
#define ST77XX_BRED       0XF81F
#define ST77XX_GRED       0XFFE0
#define ST77XX_GBLUE      0X07FF
#define ST77XX_BROWN      0XBC40
#define ST77XX_BRRED      0XFC07
#define ST77XX_GRAY       0X8430

#define TEXT_BASELINE_OFS_Y	13

extern uint8_t Icon16[];
#define ICON16x16_TITLE		&Icon16[32*0]
#define ICON16x16_ARTIST	&Icon16[32*1]
#define ICON16x16_ALBUM		&Icon16[32*2]
#define ICON16x16_FOLDER	&Icon16[32*3]
#define ICON16x16_FILE		&Icon16[32*4]
#define ICON16x16_VOLUME	&Icon16[32*5]
#define ICON16x16_BATTERY	&Icon16[32*6]

typedef enum _mode_enm {
	FileView = 0,
	Play,
	PowerOff
} mode_enm;

typedef enum _align_enm {
	AlignLeft = 0,
	AlignRight,
	AlignCenter
} align_enm;

//=================================
// Definition of Box interface
//=================================
class Box {
public:
	//virtual ~Box() {}
	virtual void setBgColor(uint16_t bgColor) = 0;
	virtual void update() = 0;
	virtual void draw(Adafruit_ST7735 *tft) = 0;
};

//=================================
// Definition of Box class < Box
//=================================
class IconBox : public Box
{
public:
	IconBox(int16_t pos_x, int16_t pos_y, uint16_t fgColor = ST77XX_WHITE, uint16_t bgColor = ST77XX_BLACK);
	IconBox(int16_t pos_x, int16_t pos_y, uint8_t *icon, uint16_t fgColor = ST77XX_WHITE, uint16_t bgColor = ST77XX_BLACK);
	void setFgColor(uint16_t fgColor);
	void setBgColor(uint16_t bgColor);
	void update();
	void draw(Adafruit_ST7735 *tft);
	void clear(Adafruit_ST7735 *tft);
	void setIcon(uint8_t *icon);
	static const int iconWidth = 16;
	static const int iconHeight = 16;
protected:
	bool isUpdated;
	int16_t pos_x, pos_y;
	uint16_t fgColor;
	uint16_t bgColor;
	uint8_t *icon;
};

//=================================
// Definition of TextBox class < Box
//=================================
class TextBox : public Box
{
public:
	TextBox(int16_t pos_x, int16_t pos_y, uint16_t fgColor = ST77XX_WHITE, uint16_t bgColor = ST77XX_BLACK);
	TextBox(int16_t pos_x, int16_t pos_y, align_enm align, uint16_t fgColor = ST77XX_WHITE, uint16_t bgColor = ST77XX_BLACK);
	TextBox(int16_t pos_x, int16_t pos_y, const char *str, align_enm align, uint16_t fgColor = ST77XX_WHITE, uint16_t bgColor = ST77XX_BLACK);
	void setFgColor(uint16_t fgColor);
	void setBgColor(uint16_t bgColor);
	virtual void update();
	void draw(Adafruit_ST7735 *tft);
	void setText(const char *str);
	void setFormatText(const char *fmt, ...);
	void setInt(int value);
	static const int charSize = 256;
protected:
	bool isUpdated;
	int16_t pos_x, pos_y;
	uint16_t fgColor;
	uint16_t bgColor;
	int16_t x0, y0; // previous rectangle origin
	uint16_t w0, h0; // previous rectangle size
	align_enm align;
	char str[charSize];
};

//=================================
// Definition of IconTextBox class < TextBox
//=================================
class IconTextBox : public TextBox
{
public:
	IconTextBox(int16_t pos_x, int16_t pos_y, uint8_t *icon, uint16_t fgColor = ST77XX_WHITE, uint16_t bgColor = ST77XX_BLACK);
	void setFgColor(uint16_t fgColor);
	void setBgColor(uint16_t bgColor);
	void update();
	void draw(Adafruit_ST7735 *tft);
	void setIcon(uint8_t *icon);
protected:
	IconBox iconBox;
};

//=================================
// Definition of ScrollTextBox class < Box
//=================================
class ScrollTextBox : public Box
{
public:
	ScrollTextBox(int16_t pos_x, int16_t pos_y, uint16_t width, uint16_t fgColor = ST77XX_WHITE, uint16_t bgColor = ST77XX_BLACK);
	void setFgColor(uint16_t fgColor);
	void setBgColor(uint16_t bgColor);
	virtual void update();
	void draw(Adafruit_ST7735 *tft);
	void setScroll(bool scr_en);
	void setText(const char *str);
	static const int charSize = 256;
protected:
	bool isUpdated;
	int16_t pos_x, pos_y;
	uint16_t fgColor;
	uint16_t bgColor;
	uint16_t w0; // to get str display width
	GFXcanvas1 *canvas;
	char str[charSize];
	uint16_t width;
	int16_t x_ofs;
	uint16_t stay_count;
	bool scr_en;
};

//=================================
// Definition of IconScrollTextBox class < ScrollTextBox
//=================================
class IconScrollTextBox : public ScrollTextBox
{
public:
	IconScrollTextBox(int16_t pos_x, int16_t pos_y, uint16_t width, uint16_t fgColor = ST77XX_WHITE, uint16_t bgColor = ST77XX_BLACK);
	void setFgColor(uint16_t fgColor);
	void setBgColor(uint16_t bgColor);
	void update();
	void draw(Adafruit_ST7735 *tft);
	void setIcon(uint8_t *icon);
protected:
	IconBox iconBox;
};

//=================================
// Definition of LcdCanvas Class < Adafruit_ST7735
//=================================
class LcdCanvas : public Adafruit_ST7735
{
public:
	LcdCanvas(int8_t cs, int8_t dc, int8_t rst);
    ~LcdCanvas();
	void clear();
	void bye();
	void setFileItem(int column, const char *str, bool isDir = false, bool isFocused = false);
	void setBitRate(uint16_t value);
	void setVolume(uint8_t value);
	void setPlayTime(uint32_t posionSec, uint32_t lengthSec);
	void switchToFileView();
	void switchToPlay();
	void draw();

protected:
	mode_enm mode;
	IconScrollTextBox fileItem[10] = {
		IconScrollTextBox(16*0, 16*0, width(), ST77XX_GRAY),
		IconScrollTextBox(16*0, 16*1, width(), ST77XX_GRAY),
		IconScrollTextBox(16*0, 16*2, width(), ST77XX_GRAY),
		IconScrollTextBox(16*0, 16*3, width(), ST77XX_GRAY),
		IconScrollTextBox(16*0, 16*4, width(), ST77XX_GRAY),
		IconScrollTextBox(16*0, 16*5, width(), ST77XX_GRAY),
		IconScrollTextBox(16*0, 16*6, width(), ST77XX_GRAY),
		IconScrollTextBox(16*0, 16*7, width(), ST77XX_GRAY),
		IconScrollTextBox(16*0, 16*8, width(), ST77XX_GRAY),
		IconScrollTextBox(16*0, 16*9, width(), ST77XX_GRAY)
	};
	IconBox battery = IconBox(16*7, 16*0, ICON16x16_BATTERY, ST77XX_GRAY);
	IconTextBox volume = IconTextBox(16*0, 16*0, ICON16x16_VOLUME, ST77XX_GRAY);
	TextBox bitRate = TextBox(16*4, 16*0, AlignCenter, ST77XX_GRAY);
	TextBox playTime = TextBox(16*8-1, 16*9, AlignRight, ST77XX_GRAY);
	ScrollTextBox title = ScrollTextBox(16*0, 16*4, width());
	TextBox artist = TextBox(0, 16*5);
	ScrollTextBox album = ScrollTextBox(0, 16*6, 128);
	Box *groupFileView[10] = {
		&fileItem[0], &fileItem[1], &fileItem[2], &fileItem[3], &fileItem[4], &fileItem[5], &fileItem[6], &fileItem[7], &fileItem[8],  &fileItem[9]
	};
	Box *groupPlay[7] = {&battery, &volume, &bitRate, &playTime, &title, &artist, &album};
	void drawFileView();
	void drawPlay();
};

#endif // __LCDCANVAS_H_INCLUDED__
