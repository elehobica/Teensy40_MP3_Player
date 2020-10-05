#ifndef __LCDCANVAS_H_INCLUDED__
#define __LCDCANVAS_H_INCLUDED__

//#define USE_ST7735
#define USE_ILI9341

#ifdef USE_ST7735
#include <Adafruit_ST7735.h> // Hardware-specific library for ST7735
#define Adafruit_LCD Adafruit_ST7735
#endif
#ifdef USE_ILI9341
#include <Adafruit_ILI9341.h> // Hardware-specific library for ILI9341
#define Adafruit_LCD Adafruit_ILI9341
#endif

//#include <Fonts/FreeMono9pt7b.h>
#include "Nimbus_Sans_L_Regular_Condensed_12.h"
#include <SdFat.h>

//#define DEBUG_LCD_CANVAS

// Common for Unicode Font & Custom Font (gfxFont)
#define FONT_HEIGHT		16

//  Custom Font (gfxFont)
#define CUSTOM_FONT	(&Nimbus_Sans_L_Regular_Condensed_12)
#define CUSTOM_FONT_OFS_Y	13

// Colors for LCD
#define LCD_WHITE         0XFFFF
#define LCD_BLACK         0X0000
#define LCD_BRED          0XF81F
#define LCD_GRED          0XFFE0
#define LCD_GBLUE         0X07FF
#define LCD_BROWN         0XBC40
#define LCD_BRRED         0XFC07
#define LCD_GRAY          0X8430

extern uint8_t Icon16[];
#define ICON16x16_TITLE		&Icon16[32*0]
#define ICON16x16_ARTIST	&Icon16[32*1]
#define ICON16x16_ALBUM		&Icon16[32*2]
#define ICON16x16_FOLDER	&Icon16[32*3]
#define ICON16x16_FILE		&Icon16[32*4]
#define ICON16x16_VOLUME	&Icon16[32*5]
#define ICON16x16_BATTERY	&Icon16[32*6]
#define ICON16x16_YEAR		&Icon16[32*7]

//=================================
// Definition of Box interface
//=================================
class Box {
public:
	typedef enum _align_enm {
		AlignLeft = 0,
		AlignRight,
		AlignCenter
	} align_enm;
	//virtual ~Box() {}
	virtual void setBgColor(uint16_t bgColor) = 0;
	virtual void update() = 0;
	virtual void draw(Adafruit_LCD *tft) = 0;
	virtual void clear(Adafruit_LCD *tft) = 0;
};

//=================================
// Definition of ImageBox class < Box
//=================================
class ImageBox : public Box
{
public:
	static const int MaxImgCnt = 4;
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
		uint16_t file_idx;
		uint64_t file_pos;
		size_t size;
	} image_t;
	ImageBox(int16_t pos_x, int16_t pos_y, uint16_t width, uint16_t height, uint16_t bgColor = LCD_BLACK);
	void setBgColor(uint16_t bgColor);
	void update();
	void draw(Adafruit_LCD *tft);
	void clear(Adafruit_LCD *tft);
	void setResizeFit(bool flg);
	void setKeepAspectRatio(bool flg);
	void setImageBuf(int16_t x, int16_t y, uint16_t rgb565);
	int addJpegBin(char *ptr, size_t size);
	int addJpegFile(uint16_t file_idx, uint64_t pos, size_t size);
	int addPngBin(char *ptr, size_t size);
	int addPngFile(uint16_t file_idx, uint64_t pos, size_t size);
	int getCount();
	void deleteAll();
	void loadNext();
	friend void cb_pngdec_draw_with_resize(void *cb_obj, uint32_t x, uint32_t y, uint16_t rgb565);
protected:
	bool isUpdated;
	int16_t pos_x, pos_y;
	uint16_t width, height; // ImageBox dimension
	uint16_t bgColor;
	uint16_t *image;
	uint16_t img_w, img_h; // dimention of image stored
	uint16_t src_w, src_h; // dimention of source image (JPEG/PNG)
	uint32_t ratio256_w, ratio256_h; // img/src ratio (128 = x0.5, 256 = x1.0, 512 = x2.0)
	bool isLoaded;
	bool changeNext;
	bool resizeFit; // true: resize to fit ImageBox size, false: original size (1:1)
	bool keepAspectRatio; // keep Aspect Ratio when resizeFit == true
	align_t align;
	int image_count;
	int image_idx;
	image_t image_array[MaxImgCnt];
	MutexFsBaseFile file;
	void jpegMcu2sAccum(int count, uint16_t mcu_w, uint16_t mcu_h, uint16_t *pImage);
	void loadJpegBin(char *ptr, size_t size);
	void loadJpegFile(uint16_t file_idx, uint64_t pos, size_t size);
	void loadJpeg(bool reduce);
	void loadPngBin(char *ptr, size_t size);
	void loadPngFile(uint16_t file_idx, uint64_t pos, size_t size);
	void loadPng(uint8_t reduce);
	void unload();
};

//=================================
// Definition of IconBox class < Box
//=================================
class IconBox : public Box
{
public:
	IconBox(int16_t pos_x, int16_t pos_y, uint16_t fgColor = LCD_WHITE, uint16_t bgColor = LCD_BLACK);
	IconBox(int16_t pos_x, int16_t pos_y, uint8_t *icon, uint16_t fgColor = LCD_WHITE, uint16_t bgColor = LCD_BLACK);
	void setFgColor(uint16_t fgColor);
	void setBgColor(uint16_t bgColor);
	void update();
	void draw(Adafruit_LCD *tft);
	void clear(Adafruit_LCD *tft);
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
// Definition of BatteryIconBox class < IconBox
//=================================
class BatteryIconBox : public IconBox
{
public:
	BatteryIconBox(int16_t pos_x, int16_t pos_y, uint16_t fgColor = LCD_WHITE, uint16_t bgColor = LCD_BLACK);
	void draw(Adafruit_LCD *tft);
	void setLevel(uint8_t value);
protected:
	uint8_t level;
};

//=================================
// Definition of TextBox class < Box
//=================================
class TextBox : public Box
{
public:
	TextBox(int16_t pos_x, int16_t pos_y, uint16_t fgColor = LCD_WHITE, uint16_t bgColor = LCD_BLACK);
	TextBox(int16_t pos_x, int16_t pos_y, align_enm align, uint16_t fgColor = LCD_WHITE, uint16_t bgColor = LCD_BLACK);
	TextBox(int16_t pos_x, int16_t pos_y, const char *str, align_enm align, uint16_t fgColor = LCD_WHITE, uint16_t bgColor = LCD_BLACK);
	void setFgColor(uint16_t fgColor);
	void setBgColor(uint16_t bgColor);
	void setEncoding(encoding_t encoding);
	void update();
	void draw(Adafruit_LCD *tft);
	void clear(Adafruit_LCD *tft);
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
	static const int BlinkInterval = 20;
	NFTextBox(int16_t pos_x, int16_t pos_y, uint16_t width, uint16_t fgColor = LCD_WHITE, uint16_t bgColor = LCD_BLACK);
	NFTextBox(int16_t pos_x, int16_t pos_y, uint16_t width, align_enm align, uint16_t fgColor = LCD_WHITE, uint16_t bgColor = LCD_BLACK);
	NFTextBox(int16_t pos_x, int16_t pos_y, uint16_t width, const char *str, align_enm align, uint16_t fgColor = LCD_WHITE, uint16_t bgColor = LCD_BLACK);
	virtual ~NFTextBox();
	void draw(Adafruit_LCD *tft);
	void clear(Adafruit_LCD *tft);
	virtual void setText(const char *str, encoding_t encoding = none);
	void setBlink(bool blink);
protected:
	GFXcanvas1 *canvas;
	uint16_t width;
	uint32_t draw_count;
	bool blink;
	void initCanvas();
};

//=================================
// Definition of IconTextBox class < TextBox
//=================================
class IconTextBox : public TextBox
{
public:
	IconTextBox(int16_t pos_x, int16_t pos_y, uint8_t *icon, uint16_t fgColor = LCD_WHITE, uint16_t bgColor = LCD_BLACK);
	void setFgColor(uint16_t fgColor);
	void setBgColor(uint16_t bgColor);
	void update();
	void draw(Adafruit_LCD *tft);
	void clear(Adafruit_LCD *tft);
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
	ScrollTextBox(int16_t pos_x, int16_t pos_y, uint16_t width, uint16_t fgColor = LCD_WHITE, uint16_t bgColor = LCD_BLACK);
	virtual ~ScrollTextBox();
	void setFgColor(uint16_t fgColor);
	void setBgColor(uint16_t bgColor);
	void update();
	void draw(Adafruit_LCD *tft);
	void clear(Adafruit_LCD *tft);
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
	IconScrollTextBox(int16_t pos_x, int16_t pos_y, uint16_t width, uint16_t fgColor = LCD_WHITE, uint16_t bgColor = LCD_BLACK);
	IconScrollTextBox(int16_t pos_x, int16_t pos_y, uint8_t *icon, uint16_t width, uint16_t fgColor = LCD_WHITE, uint16_t bgColor = LCD_BLACK);
	void setFgColor(uint16_t fgColor);
	void setBgColor(uint16_t bgColor);
	void update();
	void draw(Adafruit_LCD *tft);
	void clear(Adafruit_LCD *tft);
	void setIcon(uint8_t *icon);
protected:
	IconBox iconBox;
};

//=================================
// Definition of LcdCanvas Class < Adafruit_LCD
//=================================
class LcdCanvas : public Adafruit_LCD
{
public:
	typedef enum _mode_enm {
		FileView = 0,
		Play,
		PowerOff
	} mode_enm;

	//LcdCanvas(int8_t cs, int8_t dc, int8_t mosi, int8_t sclk, int8_t rst) : Adaruit_LCD(cs, dc, mosi, sclk, rst) {}
	LcdCanvas(int8_t cs, int8_t dc, int8_t rst);
#if !defined(ESP8266)
	//LcdCanvas(SPIClass *spiClass, int8_t cs, int8_t dc, int8_t rst) : Adaruit_LCD(spiClass, cs, dc, rst) {}
#endif // end !ESP8266

    ~LcdCanvas();
	void clear();
	void bye();
	void setFileItem(int column, const char *str, bool isDir = false, bool isFocused = false, encoding_t = none);
	void setBitRate(uint16_t value);
	void setVolume(uint8_t value);
	void setPlayTime(uint32_t posionSec, uint32_t lengthSec, bool blink = false);
	void setTrack(const char *str);
	void setTitle(const char *str, encoding_t encoding = none);
	void setAlbum(const char *str, encoding_t encoding = none);
	void setArtist(const char *str, encoding_t encoding = none);
	void setYear(const char *str, encoding_t encoding = none);
	void setBatteryVoltage(uint16_t voltage_x1000);
	void addAlbumArtJpeg(uint16_t file_idx, uint64_t pos, size_t size);
	void addAlbumArtPng(uint16_t file_idx, uint64_t pos, size_t size);
	void deleteAlbumArt();
	void switchToFileView();
	void switchToPlay();
	void switchToPowerOff(const char *msg = NULL);
	void draw();

protected:
	mode_enm mode;
	int play_count;
	const int play_cycle = 150;
	const int play_change = 100;
	IconScrollTextBox fileItem[10] = {
		IconScrollTextBox(16*0, 16*0, width(), LCD_GRAY),
		IconScrollTextBox(16*0, 16*1, width(), LCD_GRAY),
		IconScrollTextBox(16*0, 16*2, width(), LCD_GRAY),
		IconScrollTextBox(16*0, 16*3, width(), LCD_GRAY),
		IconScrollTextBox(16*0, 16*4, width(), LCD_GRAY),
		IconScrollTextBox(16*0, 16*5, width(), LCD_GRAY),
		IconScrollTextBox(16*0, 16*6, width(), LCD_GRAY),
		IconScrollTextBox(16*0, 16*7, width(), LCD_GRAY),
		IconScrollTextBox(16*0, 16*8, width(), LCD_GRAY),
		IconScrollTextBox(16*0, 16*9, width(), LCD_GRAY)
	};
	BatteryIconBox battery = BatteryIconBox(width()-16, 16*0, LCD_GRAY);
	IconTextBox volume = IconTextBox(16*0, 16*0, ICON16x16_VOLUME, LCD_GRAY);
	TextBox bitRate = TextBox(width()/2, 16*0, Box::AlignCenter, LCD_GRAY);
	NFTextBox playTime = NFTextBox(width(), height()-16, width(), Box::AlignRight, LCD_GRAY);
	IconScrollTextBox title = IconScrollTextBox(16*0, 16*3, ICON16x16_TITLE, width());
	IconScrollTextBox artist = IconScrollTextBox(16*0, 16*4, ICON16x16_ARTIST, width());
	IconScrollTextBox album = IconScrollTextBox(16*0, 16*5, ICON16x16_ALBUM, width());
	IconTextBox year = IconTextBox(16*0, 16*6, ICON16x16_YEAR);
	TextBox track = TextBox(16*0, height()-16, Box::AlignLeft, LCD_GRAY);
	TextBox bye_msg = TextBox(width()/2, height()/2-FONT_HEIGHT, "Bye", Box::AlignCenter);
	ImageBox albumArt = ImageBox(0, (height() - width())/2, width(), width());
	Box *groupFileView[10] = {
		&fileItem[0], &fileItem[1], &fileItem[2], &fileItem[3], &fileItem[4], &fileItem[5], &fileItem[6], &fileItem[7], &fileItem[8],  &fileItem[9]
	};
	Box *groupPlay[5] = {&battery, &volume, &bitRate, &playTime, &track}; // Common for Play mode 0 and 1
	Box *groupPlay0[4] = {&title, &artist, &album, &year}; // Play mode 0 only
	Box *groupPlay1[1] = {&albumArt}; // Play mode 1 only
	Box *groupPowerOff[1] = {&bye_msg};
};

#endif // __LCDCANVAS_H_INCLUDED__
