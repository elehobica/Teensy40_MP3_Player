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

typedef enum _mode_enm {
	FileView = 0,
	Play
} mode_enm;

typedef enum _align_enm {
	AlignLeft = 0,
	AlignRight,
	AlignCenter
} align_enm;

//=================================
// Definition of Box class
//=================================
class Box {
public:
	Box(uint16_t pos_x, uint16_t pos_y, uint16_t bgColor) { this->pos_x = pos_x; this->pos_y = pos_y; this->bgColor = bgColor; }
	void setBgColor(uint16_t bgColor) { if (this->bgColor == bgColor) return; this->bgColor = bgColor; isUpdated = true; }
	void update() { isUpdated = true; }
protected:
	bool isUpdated;
	uint16_t pos_x, pos_y;
	int16_t x0, y0; // previous rectangle origin
	uint16_t w0, h0; // previous rectangle size
	uint16_t bgColor;
};

//=================================
// Definition of Box class < Box
//=================================
class IconBox : public Box
{
public:
	IconBox(uint16_t pos_x, uint16_t pos_y, uint16_t bgColor = ST77XX_BLACK) : Box(pos_x, pos_y, bgColor), fgColor(ST77XX_WHITE), icon(NULL) {}
	IconBox(uint16_t pos_x, uint16_t pos_y, uint8_t *icon, uint16_t bgColor = ST77XX_BLACK) : Box(pos_x, pos_y, bgColor), fgColor(ST77XX_WHITE) { this->icon = icon; }
	void setFgColor(uint16_t fgColor) { if (this->fgColor == fgColor) return; this->fgColor = fgColor; isUpdated = true;}
	void setIcon(uint8_t *icon) { if (this->icon == icon) return; this->icon = icon; isUpdated = true; }
	void draw(Adafruit_ST7735 *tft);
	void clear(Adafruit_ST7735 *tft);
	static const int iconWidth = 16;
	static const int iconHeight = 16;
protected:
	uint16_t fgColor;
	uint8_t *icon;
};

//=================================
// Definition of TextBox class < Box
//=================================
class TextBox : public Box
{
public:
	TextBox(uint16_t pos_x, uint16_t pos_y, align_enm align = AlignLeft, uint16_t bgColor = ST77XX_BLACK) : Box(pos_x, pos_y, bgColor), fgColor(ST77XX_WHITE) { this->align = align; }
	void setFgColor(uint16_t fgColor) { if (this->fgColor == fgColor) return; this->fgColor = fgColor; isUpdated = true; }
	void setText(const char *str);
	void setFormatText(const char *fmt, ...);
	void setInt(int value);
	void draw(Adafruit_ST7735 *tft);
	static const int charSize = 256;
protected:
	uint16_t fgColor;
	align_enm align;
	char str[charSize];
};

//=================================
// Definition of IconTextBox class < IconBox & TextBox
//=================================
class IconTextBox : public IconBox, public TextBox
{
public:
	IconTextBox(uint16_t pos_x, uint16_t pos_y, align_enm align = AlignLeft, uint16_t bgColor = ST77XX_BLACK) : IconBox(pos_x, pos_y), TextBox(pos_x+IconBox::iconWidth, pos_y, align, bgColor) {}
	void setFgColor(uint16_t fgColor) { IconBox::setFgColor(fgColor); TextBox::setFgColor(fgColor); }
	void update() { IconBox::update(); TextBox::update(); }
	void draw(Adafruit_ST7735 *tft);
};

//=================================
// Definition of ScrollTextBox class < Box
//=================================
class ScrollTextBox : public Box
{
public:
	ScrollTextBox(uint16_t pos_x, uint16_t pos_y, uint16_t width, uint16_t bgColor = ST77XX_BLACK);
	void setFgColor(uint16_t fgColor) { if (this->fgColor == fgColor) return; this->fgColor = fgColor; isUpdated = true;}
	void setText(const char *str);
	void draw(Adafruit_ST7735 *tft);
	void setScroll(bool scr_en) { if (this->scr_en == scr_en) return; this->scr_en = scr_en; isUpdated = true; }
	static const int charSize = 256;
protected:
	GFXcanvas1 *canvas;
	uint16_t fgColor;
	char str[charSize];
private:
	uint16_t width;
	int16_t x_ofs;
	uint16_t stay_count;
	bool scr_en;
};

//=================================
// Definition of IconScrollTextBox class < IconBox & ScrollTextBox
//=================================
class IconScrollTextBox : public IconBox, public ScrollTextBox
{
public:
	IconScrollTextBox(uint16_t pos_x, uint16_t pos_y, uint16_t width, uint16_t bgColor = ST77XX_BLACK) : IconBox(pos_x, pos_y), ScrollTextBox(pos_x+IconBox::iconWidth, pos_y, width-IconBox::iconWidth, bgColor) {}
	void setFgColor(uint16_t fgColor) { IconBox::setFgColor(fgColor); ScrollTextBox::setFgColor(fgColor); }
	void update() { IconBox::update(); ScrollTextBox::update(); }
	void draw(Adafruit_ST7735 *tft);
};

//=================================
// Definition of LcdCanvas Class < Adafruit_ST7735
//=================================
class LcdCanvas : public Adafruit_ST7735
{
public:
	LcdCanvas(int8_t cs, int8_t dc, int8_t rst) : Adafruit_ST7735(cs, dc, rst) { init(); };
    ~LcdCanvas();
	void init();
	void clear();
	void setFileItem(int column, const char *str, bool isDir = false, bool isFocused = false);
	void setBitRate(uint16_t value);
	void setVolume(uint8_t value);
	void setPlayTime(uint32_t posionSec, uint32_t lengthSec);
	void switchToFileView();
	void switchToPlay();
	void draw();

protected:
	mode_enm mode;
	IconScrollTextBox *fileItem[10];
	IconBox *battery;
	IconTextBox *volume;
	TextBox *bitRate;
	TextBox *playTime;
	ScrollTextBox *title;
	void drawFileView();
	void drawPlay();
};

#endif // __LCDCANVAS_H_INCLUDED__
