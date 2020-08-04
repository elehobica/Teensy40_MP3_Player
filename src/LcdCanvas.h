#ifndef __LCDCANVAS_H_INCLUDED__
#define __LCDCANVAS_H_INCLUDED__

#include <Adafruit_ST7735.h> // Hardware-specific library for ST7735
#include <JPEGDecoder.h>
//#include <Fonts/FreeMono9pt7b.h>
#include "Nimbus_Sans_L_Regular_Condensed_12.h"

// Common for Unicode Font & Custom Font (gfxFont)
#define FONT_HEIGHT		16

//  Custom Font (gfxFont)
#define CUSTOM_FONT	(&Nimbus_Sans_L_Regular_Condensed_12)
#define CUSTOM_FONT_OFS_Y	13

// Additional Colors for ST77XX
#define ST77XX_BRED       0XF81F
#define ST77XX_GRED       0XFFE0
#define ST77XX_GBLUE      0X07FF
#define ST77XX_BROWN      0XBC40
#define ST77XX_BRRED      0XFC07
#define ST77XX_GRAY       0X8430

extern uint8_t Icon16[];
#define ICON16x16_TITLE		&Icon16[32*0]
#define ICON16x16_ARTIST	&Icon16[32*1]
#define ICON16x16_ALBUM		&Icon16[32*2]
#define ICON16x16_FOLDER	&Icon16[32*3]
#define ICON16x16_FILE		&Icon16[32*4]
#define ICON16x16_VOLUME	&Icon16[32*5]
#define ICON16x16_BATTERY	&Icon16[32*6]

/*
typedef enum _encoding {
	none = 0,	// ISO-8859-1
	utf8		// UTF-8
} encoding_t;
*/

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
	virtual void clear(Adafruit_ST7735 *tft) = 0;
};

//=================================
// Definition of ImageBox class < Box
//=================================
class ImageBox : public Box
{
public:
	static const int MaxImgCnt = 4;
	typedef enum _interpolation_t {
		NearestNeighbor = 0
	} interpolation_t;
	typedef enum _fitting_t {
		noFit = 0, // 1:1, no scaling
		fitXY,
		keepAspectRatio
	} fitting_t;
	typedef enum _align_t {
		origin = 0, // x=0, y=0
		center
	} align_t;
	typedef enum _media_src_t {
		char_ptr = 0,
		sdcard
	} media_src_t;
	typedef enum _img_fmt_t {
		jpeg = 0,
		png
	} img_fmt_t;
	typedef struct _image_t {
		media_src_t media_src;
		img_fmt_t img_fmt;
		char *ptr;
		FsBaseFile *file;
		uint64_t file_pos;
		size_t size;
	} image_t;
	ImageBox(int16_t pos_x, int16_t pos_y, uint16_t width, uint16_t height, uint16_t bgColor = ST77XX_BLACK);
	void setBgColor(uint16_t bgColor);
	void update();
	void draw(Adafruit_ST7735 *tft);
	void clear(Adafruit_ST7735 *tft);
	void setModes(interpolation_t interpolation, fitting_t fitting, align_t align = center);
	int addJpegBin(char *ptr, size_t size);
	int addJpegFile(FsBaseFile *file, uint64_t pos, size_t size);
	int getCount();
	void deleteAll();
	void loadNext();
protected:
	bool isUpdated;
	int16_t pos_x, pos_y;
	uint16_t width, height;
	uint16_t bgColor;
	uint16_t *image;
	uint16_t img_w, img_h;
	bool isLoaded;
	bool changeNext;
	interpolation_t interpolation;
	fitting_t fitting;
	align_t align;
	int image_count;
	int image_idx;
	image_t image_array[MaxImgCnt];
	void loadJpeg();
	void loadJpegNoFit();
	void loadJpegResize();
	void loadJpegBin(char *ptr, size_t size);
	void loadJpegFile(FsBaseFile *file, uint64_t pos, size_t size);
	void unload();
};

//=================================
// Definition of IconBox class < Box
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
	void setEncoding(encoding_t encoding);
	void update();
	void draw(Adafruit_ST7735 *tft);
	void clear(Adafruit_ST7735 *tft);
	virtual void setText(const char *str, encoding_t encoding = none);
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
	encoding_t encoding;
};

//=================================
// Definition of NFTextBox class < TextBox
// (Non-Flicker TextBox)
//=================================
class NFTextBox : public TextBox
{
public:
	NFTextBox(int16_t pos_x, int16_t pos_y, uint16_t width, uint16_t fgColor = ST77XX_WHITE, uint16_t bgColor = ST77XX_BLACK);
	NFTextBox(int16_t pos_x, int16_t pos_y, uint16_t width, align_enm align, uint16_t fgColor = ST77XX_WHITE, uint16_t bgColor = ST77XX_BLACK);
	NFTextBox(int16_t pos_x, int16_t pos_y, uint16_t width, const char *str, align_enm align, uint16_t fgColor = ST77XX_WHITE, uint16_t bgColor = ST77XX_BLACK);
	virtual ~NFTextBox();
	void draw(Adafruit_ST7735 *tft);
	void clear(Adafruit_ST7735 *tft);
	virtual void setText(const char *str, encoding_t encoding = none);
protected:
	GFXcanvas1 *canvas;
	uint16_t width;
	void initCanvas();
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
	void clear(Adafruit_ST7735 *tft);
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
	virtual ~ScrollTextBox();
	void setFgColor(uint16_t fgColor);
	void setBgColor(uint16_t bgColor);
	void update();
	void draw(Adafruit_ST7735 *tft);
	void clear(Adafruit_ST7735 *tft);
	void setScroll(bool scr_en);
	virtual void setText(const char *str, encoding_t encoding = none);
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
	encoding_t encoding;
};

//=================================
// Definition of IconScrollTextBox class < ScrollTextBox
//=================================
class IconScrollTextBox : public ScrollTextBox
{
public:
	IconScrollTextBox(int16_t pos_x, int16_t pos_y, uint16_t width, uint16_t fgColor = ST77XX_WHITE, uint16_t bgColor = ST77XX_BLACK);
	IconScrollTextBox(int16_t pos_x, int16_t pos_y, uint8_t *icon, uint16_t width, uint16_t fgColor = ST77XX_WHITE, uint16_t bgColor = ST77XX_BLACK);
	void setFgColor(uint16_t fgColor);
	void setBgColor(uint16_t bgColor);
	void update();
	void draw(Adafruit_ST7735 *tft);
	void clear(Adafruit_ST7735 *tft);
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
	//LcdCanvas(int8_t cs, int8_t dc, int8_t mosi, int8_t sclk, int8_t rst) : Adafruit_ST7735(cs, dc, mosi, sclk, rst) {}
	LcdCanvas(int8_t cs, int8_t dc, int8_t rst);
#if !defined(ESP8266)
	//LcdCanvas(SPIClass *spiClass, int8_t cs, int8_t dc, int8_t rst) : Adafruit_ST7735(spiClass, cs, dc, rst) {}
#endif // end !ESP8266

    ~LcdCanvas();
	void clear();
	void bye();
	void setFileItem(int column, const char *str, bool isDir = false, bool isFocused = false, encoding_t = none);
	void setBitRate(uint16_t value);
	void setVolume(uint8_t value);
	void setPlayTime(uint32_t posionSec, uint32_t lengthSec);
	void setTitle(const char *str, encoding_t encoding = none);
	void setAlbum(const char *str, encoding_t encoding = none);
	void setArtist(const char *str, encoding_t encoding = none);
	void addAlbumArtJpeg(FsBaseFile *file, uint64_t pos, size_t size);
	void deleteAlbumArt();
	void switchToFileView();
	void switchToPlay();
	void switchToPowerOff();
	void draw();

protected:
	mode_enm mode;
	int play_count;
	const int play_cycle = 150;
	const int play_change = 100;
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
	NFTextBox playTime = NFTextBox(width(), 16*9, width(), AlignRight, ST77XX_GRAY);
	IconScrollTextBox title = IconScrollTextBox(16*0, 16*3, ICON16x16_TITLE, width());
	IconScrollTextBox artist = IconScrollTextBox(16*0, 16*4, ICON16x16_ARTIST, width());
	IconScrollTextBox album = IconScrollTextBox(16*0, 16*5, ICON16x16_ALBUM, width());
	TextBox bye_msg = TextBox(width()/2, height()/2-FONT_HEIGHT, "Bye", AlignCenter);
	ImageBox albumArt = ImageBox(0, (height() - width())/2, width(), width());
	Box *groupFileView[10] = {
		&fileItem[0], &fileItem[1], &fileItem[2], &fileItem[3], &fileItem[4], &fileItem[5], &fileItem[6], &fileItem[7], &fileItem[8],  &fileItem[9]
	};
	Box *groupPlay[4] = {&battery, &volume, &bitRate, &playTime}; // Common for Play mode 0 and 1
	Box *groupPlay0[3] = {&title, &artist, &album}; // Play mode 0 only
	Box *groupPlay1[1] = {&albumArt}; // Play mode 1 only
	Box *groupPowerOff[1] = {&bye_msg};
};

#endif // __LCDCANVAS_H_INCLUDED__
