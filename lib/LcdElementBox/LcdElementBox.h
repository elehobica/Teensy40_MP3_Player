/*-----------------------------------------------------------/
/ LcdElementBox: Lcd Element Box API for Adafruit_SPITFT v0.90
/------------------------------------------------------------/
/ Copyright (c) 2020, Elehobica
/ Released under the BSD-2-Clause
/ refer to https://opensource.org/licenses/BSD-2-Clause
/-----------------------------------------------------------*/

#ifndef __LCDELEMENTBOX_H_INCLUDED__
#define __LCDELEMENTBOX_H_INCLUDED__

#include <Adafruit_SPITFT.h>
#include <SdFat.h>

//#define DEBUG_LCD_ELEMENT_BOX

// Colors for LCD
#define LCD_WHITE         0XFFFF
#define LCD_BLACK         0X0000
#define LCD_BRED          0XF81F
#define LCD_GRED          0XFFE0
#define LCD_GBLUE         0X07FF
#define LCD_BROWN         0XBC40
#define LCD_BRRED         0XFC07
#define LCD_GRAY          0X8430

//=================================
// Definition of LcdElementBox interface
//=================================
class LcdElementBox {
public:
	typedef enum _align_enm {
		AlignLeft = 0,
		AlignRight,
		AlignCenter
	} align_enm;
	//virtual ~LcdElementBox() {}
	virtual void setBgColor(uint16_t bgColor) = 0;
	virtual void update() = 0;
	virtual void draw(Adafruit_SPITFT *tft) = 0;
	virtual void clear(Adafruit_SPITFT *tft) = 0;
};

//=================================
// Definition of ImageBox class < LcdElementBox
//=================================
class ImageBox : public LcdElementBox
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
		bool is_unsync;
	} image_t;
	ImageBox(int16_t pos_x, int16_t pos_y, uint16_t width, uint16_t height, uint16_t bgColor = LCD_BLACK);
	void setBgColor(uint16_t bgColor);
	void update();
	void draw(Adafruit_SPITFT *tft);
	void clear(Adafruit_SPITFT *tft);
	void setResizeFit(bool flg);
	void setKeepAspectRatio(bool flg);
	void setImageBuf(int16_t x, int16_t y, uint16_t rgb565);
	int addJpegBin(char *ptr, size_t size);
	int addJpegFile(uint16_t file_idx, uint64_t pos, size_t size, bool is_unsync = false);
	int addPngBin(char *ptr, size_t size);
	int addPngFile(uint16_t file_idx, uint64_t pos, size_t size, bool is_unsync = false);
	int getCount();
	void deleteAll();
	bool loadNext();
	friend void cb_pngdec_draw_with_resize(void *cb_obj, uint32_t x, uint32_t y, uint16_t rgb565);
protected:
	bool isUpdated;
	int16_t pos_x, pos_y;
	uint16_t width, height; // ImageBox dimension
	uint16_t bgColor;
	bool decode_ok;
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
	bool loadJpegBin(char *ptr, size_t size);
	bool loadJpegFile(uint16_t file_idx, uint64_t pos, size_t size, bool is_unsync = false);
	void loadJpeg(bool reduce);
	bool loadPngBin(char *ptr, size_t size);
	bool loadPngFile(uint16_t file_idx, uint64_t pos, size_t size, bool is_unsync = false);
	void loadPng(uint8_t reduce);
	void unload();
};

//=================================
// Definition of IconBox class < LcdElementBox
//=================================
class IconBox : public LcdElementBox
{
public:
	IconBox(int16_t pos_x, int16_t pos_y, uint16_t fgColor = LCD_WHITE, uint16_t bgColor = LCD_BLACK);
	IconBox(int16_t pos_x, int16_t pos_y, uint8_t *icon, uint16_t fgColor = LCD_WHITE, uint16_t bgColor = LCD_BLACK);
	void setFgColor(uint16_t fgColor);
	void setBgColor(uint16_t bgColor);
	void update();
	void draw(Adafruit_SPITFT *tft);
	void clear(Adafruit_SPITFT *tft);
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
// Definition of TextBox class < LcdElementBox
//=================================
class TextBox : public LcdElementBox
{
public:
	TextBox(int16_t pos_x, int16_t pos_y, uint16_t fgColor = LCD_WHITE, uint16_t bgColor = LCD_BLACK);
	TextBox(int16_t pos_x, int16_t pos_y, align_enm align, uint16_t fgColor = LCD_WHITE, uint16_t bgColor = LCD_BLACK);
	TextBox(int16_t pos_x, int16_t pos_y, const char *str, align_enm align, uint16_t fgColor = LCD_WHITE, uint16_t bgColor = LCD_BLACK);
	void setFgColor(uint16_t fgColor);
	void setBgColor(uint16_t bgColor);
	void setEncoding(encoding_t encoding);
	void update();
	void draw(Adafruit_SPITFT *tft);
	void clear(Adafruit_SPITFT *tft);
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
	NFTextBox(int16_t pos_x, int16_t pos_y, uint16_t width, uint16_t fgColor = LCD_WHITE, uint16_t bgColor = LCD_BLACK, const GFXfont *f = NULL, int16_t ofs_y = 0, uint16_t height_y = 16);
	NFTextBox(int16_t pos_x, int16_t pos_y, uint16_t width, align_enm align, uint16_t fgColor = LCD_WHITE, uint16_t bgColor = LCD_BLACK, const GFXfont *f = NULL, int16_t ofs_y = 0, uint16_t height_y = 16);
	NFTextBox(int16_t pos_x, int16_t pos_y, uint16_t width, const char *str, align_enm align, uint16_t fgColor = LCD_WHITE, uint16_t bgColor = LCD_BLACK, const GFXfont *f = NULL, int16_t ofs_y = 0, uint16_t height_y = 16);
	virtual ~NFTextBox();
	void draw(Adafruit_SPITFT *tft);
	void clear(Adafruit_SPITFT *tft);
	virtual void setText(const char *str, encoding_t encoding = none);
	void setBlink(bool blink);
protected:
	GFXcanvas1 *canvas;
	const GFXfont *Font;
	int16_t ofs_y;
	uint16_t height_y;
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
	void draw(Adafruit_SPITFT *tft);
	void clear(Adafruit_SPITFT *tft);
	void setIcon(uint8_t *icon);
protected:
	IconBox iconBox;
};

//=================================
// Definition of ScrollTextBox class < LcdElementBox
//=================================
class ScrollTextBox : public LcdElementBox
{
public:
	ScrollTextBox(int16_t pos_x, int16_t pos_y, uint16_t width, uint16_t fgColor = LCD_WHITE, uint16_t bgColor = LCD_BLACK, const GFXfont *f = NULL, int16_t ofs_y = 0, uint16_t height_y = 16);
	virtual ~ScrollTextBox();
	void setFgColor(uint16_t fgColor);
	void setBgColor(uint16_t bgColor);
	void update();
	void draw(Adafruit_SPITFT *tft);
	void clear(Adafruit_SPITFT *tft);
	void setScroll(bool scr_en);
	virtual void setText(const char *str, encoding_t encoding = none);
	static const int charSize = 256;
protected:
	bool isUpdated;
	int16_t pos_x, pos_y;
	uint16_t fgColor;
	uint16_t bgColor;
	const GFXfont *Font;
	int16_t ofs_y;
	uint16_t height_y;
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
	IconScrollTextBox(int16_t pos_x, int16_t pos_y, uint16_t width, uint16_t fgColor = LCD_WHITE, uint16_t bgColor = LCD_BLACK, const GFXfont *f = NULL, int16_t ofs_y = 0, uint16_t height_y = 16);
	IconScrollTextBox(int16_t pos_x, int16_t pos_y, uint8_t *icon, uint16_t width, uint16_t fgColor = LCD_WHITE, uint16_t bgColor = LCD_BLACK, const GFXfont *f = NULL, int16_t ofs_y = 0, uint16_t height_y = 16);
	void setFgColor(uint16_t fgColor);
	void setBgColor(uint16_t bgColor);
	void update();
	void draw(Adafruit_SPITFT *tft);
	void clear(Adafruit_SPITFT *tft);
	void setIcon(uint8_t *icon);
protected:
	IconBox iconBox;
};

#endif // __LCDELEMENTBOX_H_INCLUDED__
