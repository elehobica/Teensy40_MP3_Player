#include "LcdCanvas.h"
#include "iconfont.h"

//=================================
// Implementation of ImageBox class
//=================================
ImageBox::ImageBox(int16_t pos_x, int16_t pos_y, uint16_t width, uint16_t height, uint16_t bgColor)
    : isUpdated(true), pos_x(pos_x), pos_y(pos_y), width(width), height(height), bgColor(bgColor),
        image(NULL), img_w(0), img_h(0), isImageLoaded(false), interpolation(NearestNeighbor), fitting(keepAspectRatio), align(center)
{
    image = (uint16_t *) calloc(2, width * height);
}

void ImageBox::setBgColor(uint16_t bgColor)
{
    if (this->bgColor == bgColor) { return; }
    this->bgColor = bgColor;
    update();
}

void ImageBox::update()
{
    isUpdated = true;
}

void ImageBox::draw(Adafruit_ST7735 *tft)
{
    if (!isUpdated || image == NULL) { return; }
    isUpdated = false;
    int16_t ofs_x = 0;
    int16_t ofs_y = 0;

    if (align == center) {
        ofs_x = (width-img_w)/2;
        ofs_y = (height-img_h)/2;
    }
    tft->drawRGBBitmap(pos_x+ofs_x, pos_y+ofs_y, image, img_w, img_h);
}

void ImageBox::clear(Adafruit_ST7735 *tft)
{
    tft->fillRect(pos_x, pos_y, width, height, bgColor); // clear Icon rectangle
}

void ImageBox::setModes(interpolation_t interpolation, fitting_t fitting, align_t align)
{
    this->interpolation = interpolation;
    this->fitting = fitting;
    this->align = align;
}

// load from JPEG binary
void ImageBox::loadJpegBin(char *ptr, size_t size)
{
    bool decoded = JpegDec.decodeArray((const uint8_t *) ptr, (uint32_t) size);
    if (!decoded) { return; }
    loadJpeg();
}

// load from JPEG File
void ImageBox::loadJpegFile(FsBaseFile *file, uint64_t pos, size_t size)
{
    bool decoded = JpegDec.decodeSdFile(*file, pos, size);
    if (!decoded) { return; }
    loadJpeg();
}

// load JPEG to fit to width/height by Nearest Neighbor
void ImageBox::loadJpeg()
{
    uint16_t jpg_w = JpegDec.width;
    uint16_t jpg_h = JpegDec.height;
    uint16_t mcu_w = JpegDec.MCUWidth;
    uint16_t mcu_h = JpegDec.MCUHeight;
    { // DEBUG
        char str[256];
        sprintf(str, "JPEG info: (w, h) = (%d, %d), (mcu_w, mcu_h) = (%d, %d)", jpg_w, jpg_h, mcu_w, mcu_h);
        Serial.println(str);
    }
    while (JpegDec.read()) {
        int idx = 0;
        int16_t x, y;
        int16_t mcu_x = JpegDec.MCUx;
        int16_t mcu_y = JpegDec.MCUy;
        int16_t mod_y_pls = (fitting != keepAspectRatio || jpg_h >= jpg_w) ? jpg_h : jpg_w;
        int16_t mod_y = mod_y_pls;
        int16_t plot_y = 0;
        // prepare plot_y (, mod_y) condition
        if (fitting == noFit) {
            plot_y = mcu_h * mcu_y;
            if (plot_y >= height) continue; // don't use break because MCU order is not always left-to-right and top-to-bottom
        } else {
            for (y = 0; y < mcu_h * mcu_y; y++) {
                while (mod_y < 0) {
                    mod_y += mod_y_pls;
                    plot_y++;
                }
                mod_y -= height;
            }
        }
        for (int16_t mcu_ofs_y = 0; mcu_ofs_y < mcu_h; mcu_ofs_y++) {
            y = mcu_h * mcu_y + mcu_ofs_y;
            if (y >= jpg_h) break;
            int16_t mod_x_pls = (fitting != keepAspectRatio || jpg_w >= jpg_h) ? jpg_w : jpg_h;
            int16_t mod_x = mod_x_pls;
            int16_t plot_x = 0;
            int16_t plot_start_x = 0;
            // prepare plot_x (, mod_x) condition
            if (fitting == noFit) {
                plot_x = mcu_w * mcu_x;
                if (plot_x >= width) break;
            } else {
                for (x = 0; x < mcu_w * mcu_x; x++) {
                    while (mod_x < 0) {
                        mod_x += mod_x_pls;
                        plot_x++;
                    }
                    mod_x -= width;
                }
                plot_start_x = plot_x;
            }
            // actual plot_x
            for (int16_t mcu_ofs_x = 0; mcu_ofs_x < mcu_w; mcu_ofs_x++) {
                x = mcu_w * mcu_x + mcu_ofs_x;
                if (x >= jpg_w) {
                    idx += mcu_w - jpg_w%mcu_w; // skip horizontal padding area
                    break;
                }
                if (fitting == noFit) {
                    if (plot_x >= width) break;
                    image[width*plot_y+plot_x] = JpegDec.pImage[idx];
                    if (plot_x+1 > img_w) { img_w = plot_x+1; }
                    plot_x++;
                } else {
                    while (mod_x < 0) {
                        if (plot_x >= width) break;
                        if (mod_y < 0) {
                            { // DEBUG
                                char str[256];
                                sprintf(str, "plot_x, plot_y = %d, %d", plot_x, plot_y);
                            }
                            image[width*plot_y+plot_x] = JpegDec.pImage[idx];
                            if (plot_x+1 > img_w) { img_w = plot_x+1; }
                        }
                        mod_x += mod_x_pls;
                        plot_x++;
                    }
                    mod_x -= width;
                }
                idx++;
            }
            if (fitting == noFit) {
                if (plot_y+1 > img_h) { img_h = plot_y+1; }
                plot_y++;
                if (plot_y >= height) break;
            } else {
                while (mod_y < 0) { // repeat previous line in case of expanding
                    if (plot_y+1 > img_h) { img_h = plot_y+1; }
                    mod_y += mod_y_pls;
                    plot_y++;
                    if (plot_y >= height) break;
                    if (mod_y < 0) {
                        for (x = plot_start_x; x < plot_x; x++) {
                            image[width*plot_y+x] = image[width*(plot_y-1)+x];
                        }
                    }
                }
                mod_y -= height;
            }
        }
    }
    { // DEBUG
        char str[256];
        sprintf(str, "Resized to (img_w, img_h) = (%d, %d)", img_w, img_h);
        Serial.println(str);
    }
    if (img_w < width) { // delete horizontal blank
        for (int16_t plot_y = 1; plot_y < img_h; plot_y++) {
            memmove(&image[img_w*plot_y], &image[width*plot_y], img_w*2);
        }
    }
    isImageLoaded = true;
    update();
}

void ImageBox::unload()
{
    img_w = img_h = 0;
    memset(image, 0, width * height * 2);
    isImageLoaded = false;
}

bool ImageBox::isLoaded()
{
    return isImageLoaded;
}

//=================================
// Implementation of IconBox class
//=================================
IconBox::IconBox(int16_t pos_x, int16_t pos_y, uint16_t fgColor, uint16_t bgColor)
    : isUpdated(true), pos_x(pos_x), pos_y(pos_y), fgColor(fgColor), bgColor(bgColor), icon(NULL) {}

IconBox::IconBox(int16_t pos_x, int16_t pos_y, uint8_t *icon, uint16_t fgColor, uint16_t bgColor)
    : isUpdated(true), pos_x(pos_x), pos_y(pos_y), fgColor(fgColor), bgColor(bgColor), icon(icon) {}

void IconBox::setFgColor(uint16_t fgColor)
{
    if (this->fgColor == fgColor) { return; }
    this->fgColor = fgColor;
    update();
}

void IconBox::setBgColor(uint16_t bgColor)
{
    if (this->bgColor == bgColor) { return; }
    this->bgColor = bgColor;
    update();
}

void IconBox::update()
{
    isUpdated = true;
}

void IconBox::draw(Adafruit_ST7735 *tft)
{
    if (!isUpdated) { return; }
    isUpdated = false;
    clear(tft);
    if (icon != NULL) {
        tft->drawBitmap(pos_x, pos_y, icon, iconWidth, iconHeight, fgColor);
    }
}

void IconBox::clear(Adafruit_ST7735 *tft)
{
    tft->fillRect(pos_x, pos_y, iconWidth, iconHeight, bgColor); // clear Icon rectangle
}

void IconBox::setIcon(uint8_t *icon)
{
    if (this->icon == icon) { return; }
    this->icon = icon;
    update();
}

//=================================
// Implementation of TextBox class
//=================================
TextBox::TextBox(int16_t pos_x, int16_t pos_y, uint16_t fgColor, uint16_t bgColor)
    : isUpdated(true), pos_x(pos_x), pos_y(pos_y), fgColor(fgColor), bgColor(bgColor), align(AlignLeft), str(""), encoding(none) {}

TextBox::TextBox(int16_t pos_x, int16_t pos_y, align_enm align, uint16_t fgColor, uint16_t bgColor)
    : isUpdated(true), pos_x(pos_x), pos_y(pos_y), fgColor(fgColor), bgColor(bgColor), align(align), str(""), encoding(none) {}

TextBox::TextBox(int16_t pos_x, int16_t pos_y, const char *str, align_enm align, uint16_t fgColor, uint16_t bgColor)
    : isUpdated(true), pos_x(pos_x), pos_y(pos_y), fgColor(fgColor), bgColor(bgColor), align(align), str(""), encoding(none)
{
    setText(str);
}

void TextBox::setFgColor(uint16_t fgColor)
{
    if (this->fgColor == fgColor) { return; }
    this->fgColor = fgColor;
    update();
}

void TextBox::setBgColor(uint16_t bgColor)
{
    if (this->bgColor == bgColor) { return; }
    this->bgColor = bgColor;
    update();
}

void TextBox::setEncoding(encoding_t encoding)
{
    this->encoding = encoding;
}

void TextBox::update()
{
    isUpdated = true;
}

void TextBox::draw(Adafruit_ST7735 *tft)
{
    if (!isUpdated) { return; }
    int16_t x_ofs;
    isUpdated = false;
    TextBox::clear(tft); // call clear() of this class
    //tft->fillRect(x0, y0, w0, h0, bgColor); // clear previous rectangle
    tft->setTextColor(fgColor);
    x_ofs = (align == AlignRight) ? -w0 : (align == AlignCenter) ? -w0/2 : 0; // at this point, w0 could be not correct
    tft->getTextBounds(str, pos_x+x_ofs, pos_y, &x0, &y0, &w0, &h0, encoding); // update next smallest rectangle (get correct w0)
    x_ofs = (align == AlignRight) ? -w0 : (align == AlignCenter) ? -w0/2 : 0;
    tft->getTextBounds(str, pos_x+x_ofs, pos_y, &x0, &y0, &w0, &h0, encoding); // update next smallest rectangle (get total correct info)
    tft->setCursor(pos_x+x_ofs, pos_y);
    tft->println(str, encoding);
}

void TextBox::clear(Adafruit_ST7735 *tft)
{
    tft->fillRect(x0, y0, w0, h0, bgColor); // clear previous rectangle
}

void TextBox::setText(const char *str, encoding_t encoding)
{
    if (strncmp(this->str, str, charSize) == 0) { return; }
    //strncpy(this->str, str, charSize);
    memcpy(this->str, str, charSize);
    this->encoding = encoding;
    update();
}

void TextBox::setFormatText(const char *fmt, ...)
{
    char str_temp[charSize];
    va_list va;
    va_start(va, fmt);
    vsprintf(str_temp, fmt, va);
    va_end(va);
    setText(str_temp, encoding);
}

void TextBox::setInt(int value)
{
    setEncoding(none);
    setFormatText("%d", value);
}

//=================================
// Implementation of NFTextBox class
//=================================
NFTextBox::NFTextBox(int16_t pos_x, int16_t pos_y, uint16_t width, uint16_t fgColor, uint16_t bgColor)
    : TextBox(pos_x, pos_y, "", AlignLeft, fgColor, bgColor), width(width)
{
    initCanvas();
}

NFTextBox::NFTextBox(int16_t pos_x, int16_t pos_y, uint16_t width, align_enm align, uint16_t fgColor, uint16_t bgColor)
    : TextBox(pos_x, pos_y, "", align, fgColor, bgColor), width(width)
{
    initCanvas();
}

NFTextBox::NFTextBox(int16_t pos_x, int16_t pos_y, uint16_t width, const char *str, align_enm align, uint16_t fgColor, uint16_t bgColor)
    : TextBox(pos_x, pos_y, str, align, fgColor, bgColor), width(width)
{
    initCanvas();
}

NFTextBox::~NFTextBox()
{
    // deleting object of polymorphic class type 'GFXcanvas1' which has non-virtual destructor might cause undefined behaviour
    //delete canvas;
}

void NFTextBox::initCanvas()
{
    canvas = new GFXcanvas1(width, FONT_HEIGHT); // 2x width for minimize canvas update, which costs Unifont SdFat access
    canvas->setFont(CUSTOM_FONT, CUSTOM_FONT_OFS_Y);
    canvas->setTextWrap(false);
    canvas->setTextSize(1);
    canvas->getTextBounds(str, 0, 0, &x0, &y0, &w0, &h0, encoding); // idle-run because first time fails somehow
}

void NFTextBox::draw(Adafruit_ST7735 *tft)
{
    if (!isUpdated) { return; }
    int16_t x_ofs;
    int16_t new_x_ofs;
    uint16_t new_w0;
    isUpdated = false;
    x_ofs = (align == AlignRight) ? -w0 : (align == AlignCenter) ? -w0/2 : 0; // previous x_ofs
    canvas->getTextBounds(str, 0, 0, &x0, &y0, &new_w0, &h0, encoding); // get next smallest rectangle (get correct new_w0)
    new_x_ofs = (align == AlignRight) ? -new_w0 : (align == AlignCenter) ? -new_w0/2 : 0;
    if (new_x_ofs > x_ofs) { // clear left-over rectangle
        tft->fillRect(pos_x+x_ofs, pos_y+y0, new_x_ofs-x_ofs, h0, bgColor);
    }
    if (new_x_ofs+new_w0 < x_ofs+w0) { // clear right-over rectangle
        tft->fillRect(pos_x+new_x_ofs+new_w0, pos_y+y0, x_ofs+w0-new_x_ofs-new_w0, h0, bgColor);
    }
    // update info
    w0 = new_w0;
    x_ofs = new_x_ofs;
    // Flicker less draw (width must be NFTextBox's width)
    //tft->drawBitmap(pos_x+x_ofs, pos_y, canvas->getBuffer(), width, FONT_HEIGHT, fgColor, bgColor);
    {
        int16_t x = pos_x+x_ofs;
        int16_t y = pos_y;
        const uint8_t *bitmap = canvas->getBuffer();
        int16_t w = width;
        int16_t h = FONT_HEIGHT;
        uint16_t color = fgColor;
        uint16_t bg = bgColor;

        int16_t byteWidth = (w + 7) / 8; // Bitmap scanline pad = whole byte
        uint8_t byte = 0;

        tft->startWrite();
        for (int16_t j = 0; j < h; j++, y++) {
            for (int16_t i = 0; i < w; i++) {
                if (i & 7) {
                    byte <<= 1;
                } else {
                    byte = pgm_read_byte(&bitmap[j * byteWidth + i / 8]);
                }
                if (x+i >= pos_x+x_ofs && x+i < pos_x+x_ofs+w0) {
                    tft->writePixel(x+i, y, (byte & 0x80) ? color : bg);
                }
            }
        }
        tft->endWrite();
    }
}

void NFTextBox::clear(Adafruit_ST7735 *tft)
{
    int16_t x_ofs = (align == AlignRight) ? -w0 : (align == AlignCenter) ? -w0/2 : 0; // previous x_ofs
    tft->fillRect(pos_x+x_ofs, pos_y+y0, w0, h0, bgColor);
}

void NFTextBox::setText(const char *str, encoding_t encoding)
{
    if (strncmp(this->str, str, charSize) == 0) { return; }
    this->encoding = encoding;
    update();
    //strncpy(this->str, str, charSize);
    memcpy(this->str, str, charSize);
    canvas->fillRect(0, 0, width, FONT_HEIGHT, bgColor);
    canvas->setCursor(0, 0);
    canvas->print(str, encoding);
}

//=================================
// Implementation of IconTextBox class
//=================================
IconTextBox::IconTextBox(int16_t pos_x, int16_t pos_y, uint8_t *icon, uint16_t fgColor, uint16_t bgColor)
    : TextBox(pos_x+16, pos_y, fgColor, bgColor), iconBox(pos_x, pos_y, icon, fgColor, bgColor) {}

void IconTextBox::setFgColor(uint16_t fgColor)
{
    iconBox.setFgColor(fgColor);
    TextBox::setFgColor(fgColor);
}

void IconTextBox::setBgColor(uint16_t bgColor)
{
    iconBox.setBgColor(bgColor);
    TextBox::setBgColor(bgColor);
}

void IconTextBox::update()
{
    iconBox.update();
    TextBox::update();
}

void IconTextBox::draw(Adafruit_ST7735 *tft)
{
    // For IconBox: Don't display IconBox if str of TextBox is ""
    if (strlen(str) == 0) {
        if (isUpdated) { iconBox.clear(tft); }
    } else {
        iconBox.draw(tft);
    }
    // For TextBox
    TextBox::draw(tft);
}

void IconTextBox::clear(Adafruit_ST7735 *tft)
{
    iconBox.clear(tft);
    TextBox::clear(tft);
}

void IconTextBox::setIcon(uint8_t *icon)
{
    iconBox.setIcon(icon);
}

//=================================
// Implementation of ScrollTextBox class < Box
//=================================
ScrollTextBox::ScrollTextBox(int16_t pos_x, int16_t pos_y, uint16_t width, uint16_t fgColor, uint16_t bgColor)
    : isUpdated(true), pos_x(pos_x), pos_y(pos_y), fgColor(fgColor), bgColor(bgColor),
      str(""), width(width), stay_count(0), scr_en(true), encoding(none)
{
    int16_t x0, y0; // dummy
    uint16_t h0; // dummy
    canvas = new GFXcanvas1(width*2, FONT_HEIGHT); // 2x width for minimize canvas update, which costs Unifont SdFat access
    canvas->setFont(CUSTOM_FONT, CUSTOM_FONT_OFS_Y);
    canvas->setTextWrap(false);
    canvas->setTextSize(1);
    canvas->getTextBounds(str, 0, 0, &x0, &y0, &w0, &h0, encoding); // idle-run because first time fails somehow
}

ScrollTextBox::~ScrollTextBox()
{
    // deleting object of polymorphic class type 'GFXcanvas1' which has non-virtual destructor might cause undefined behaviour
    //delete canvas;
}

void ScrollTextBox::setFgColor(uint16_t fgColor)
{
    if (this->fgColor == fgColor) { return; }
    this->fgColor = fgColor;
    update();
}

void ScrollTextBox::setBgColor(uint16_t bgColor)
{
    if (this->bgColor == bgColor) { return; }
    this->bgColor = bgColor;
    update();
}

void ScrollTextBox::update()
{
    isUpdated = true;
}

// canvas update policy:
//  x_ofs is minus value
//  canvas width: width * 2
//  canvas update: every ((x_ofs % width) == -(width-1))
void ScrollTextBox::draw(Adafruit_ST7735 *tft)
{
    int16_t under_offset = tft->width()-pos_x-w0; // max minus offset for scroll (width must be tft's width)
    if (!isUpdated && (under_offset >= 0 || !scr_en)) { return; }
    if (under_offset >= 0 || !scr_en) {
        x_ofs = 0;
        stay_count = 0;
    } else {
        if (x_ofs == 0 || x_ofs == under_offset) { // scroll stay a while at both end
            if (stay_count++ > 20) {
                if (x_ofs-- != 0) {
                    if (x_ofs < -width + 1) {
                        canvas->fillRect(0, 0, width*2, FONT_HEIGHT, bgColor);
                        canvas->setCursor(0, 0);
                        canvas->print(str, encoding);
                    }
                    x_ofs = 0;
                }
                stay_count = 0;
            }
        } else {
            if ((x_ofs % width) == -(width-1)) { // modulo returns minus value (C++)
                canvas->fillRect(0, 0, width*2, FONT_HEIGHT, bgColor);
                canvas->setCursor((x_ofs-1)/width*width, 0);
                canvas->print(str, encoding);
            }
            x_ofs--;
        }
    }
    if (!isUpdated && stay_count != 0) { return; }
    isUpdated = false;

    // Flicker less draw (width must be ScrollTextBox's width)
    //tft->drawBitmap(pos_x+x_ofs, pos_y, canvas->getBuffer(), width, FONT_HEIGHT, fgColor, bgColor);
    {
        int16_t x = pos_x + x_ofs%width;
        int16_t y = pos_y;
        const uint8_t *bitmap = canvas->getBuffer();
        int16_t w = width * 2;
        int16_t h = FONT_HEIGHT;
        uint16_t color = fgColor;
        uint16_t bg = bgColor;

        int16_t byteWidth = (w + 7) / 8; // Bitmap scanline pad = whole byte
        uint8_t byte = 0;

        tft->startWrite();
        for (int16_t j = 0; j < h; j++, y++) {
            for (int16_t i = 0; i < w; i++) {
                if (i & 7) {
                    byte <<= 1;
                } else {
                    byte = pgm_read_byte(&bitmap[j * byteWidth + i / 8]);
                }
                if (x+i >= pos_x) {
                    tft->writePixel(x+i, y, (byte & 0x80) ? color : bg);
                }
            }
        }
        tft->endWrite();
    }
}

void ScrollTextBox::clear(Adafruit_ST7735 *tft)
{
    tft->fillRect(pos_x, pos_y, width, FONT_HEIGHT, bgColor);
}

void ScrollTextBox::setScroll(bool scr_en)
{
    if (this->scr_en == scr_en) { return; }
    this->scr_en = scr_en;
    update();
}

void ScrollTextBox::setText(const char *str, encoding_t encoding)
{
    int16_t x0, y0; // dummy
    uint16_t h0; // dummy
    if (strncmp(this->str, str, charSize) == 0) { return; }
    this->encoding = encoding;
    update();
    //strncpy(this->str, str, charSize);
    memcpy(this->str, str, charSize);
    canvas->getTextBounds(str, 0, 0, &x0, &y0, &w0, &h0, encoding); // get width (w0)
    x_ofs = 0;
    stay_count = 0;

    canvas->fillRect(0, 0, width*2, FONT_HEIGHT, bgColor);
    canvas->setCursor(0, 0);
    canvas->print(str, encoding);
}

//=================================
// Implementation of IconScrollTextBox class < ScrollTextBox
//=================================
IconScrollTextBox::IconScrollTextBox(int16_t pos_x, int16_t pos_y, uint16_t width, uint16_t fgColor, uint16_t bgColor)
    : ScrollTextBox(pos_x+16, pos_y, width-16, fgColor, bgColor), iconBox(pos_x, pos_y, fgColor, bgColor) {}

IconScrollTextBox::IconScrollTextBox(int16_t pos_x, int16_t pos_y, uint8_t *icon, uint16_t width, uint16_t fgColor, uint16_t bgColor)
    : ScrollTextBox(pos_x+16, pos_y, width-16, fgColor, bgColor), iconBox(pos_x, pos_y, icon, fgColor, bgColor) {}

void IconScrollTextBox::setFgColor(uint16_t fgColor)
{
    iconBox.setFgColor(fgColor);
    ScrollTextBox::setFgColor(fgColor);
}

void IconScrollTextBox::setBgColor(uint16_t bgColor)
{
    iconBox.setBgColor(bgColor);
    ScrollTextBox::setBgColor(bgColor);
}

void IconScrollTextBox::update()
{
    iconBox.update();
    ScrollTextBox::update();
}

void IconScrollTextBox::draw(Adafruit_ST7735 *tft)
{
    // For IconBox: Don't display IconBox if str of ScrollTextBox is ""
    if (strlen(str) == 0) {
        if (isUpdated) { iconBox.clear(tft); }
    } else {
        iconBox.draw(tft);
    }
    // For ScrollTextBox
    ScrollTextBox::draw(tft);
}

void IconScrollTextBox::clear(Adafruit_ST7735 *tft)
{
    iconBox.clear(tft);
    ScrollTextBox::clear(tft);
}

void IconScrollTextBox::setIcon(uint8_t *icon)
{
    iconBox.setIcon(icon);
}

//=================================
// Implementation of LcdCanvas class
//=================================
LcdCanvas::LcdCanvas(int8_t cs, int8_t dc, int8_t rst) : Adafruit_ST7735(cs, dc, rst), mode(FileView), play_count(0)
{
    initR(INITR_BLACKTAB);      // Init ST7735S chip, black tab
    setTextWrap(false);
    fillScreen(ST77XX_BLACK);
    setFont(CUSTOM_FONT, CUSTOM_FONT_OFS_Y);
    setTextSize(1);
    //Serial.println(width());
    //Serial.println(height());

    // FileView parts (nothing to set here)

    // Play parts (nothing to set here)
}

LcdCanvas::~LcdCanvas()
{
}

void LcdCanvas::switchToFileView()
{
    mode = FileView;
    clear();
    for (int i = 0; i < (int) (sizeof(groupFileView)/sizeof(*groupFileView)); i++) {
        groupFileView[i]->update();
    }
}

void LcdCanvas::switchToPlay()
{
    mode = Play;
    clear();
    for (int i = 0; i < (int) (sizeof(groupPlay)/sizeof(*groupPlay)); i++) {
        groupPlay[i]->update();
    }
    for (int i = 0; i < (int) (sizeof(groupPlay0)/sizeof(*groupPlay0)); i++) {
        groupPlay0[i]->update();
    }
    for (int i = 0; i < (int) (sizeof(groupPlay1)/sizeof(*groupPlay1)); i++) {
        groupPlay1[i]->update();
    }
    play_count = 0;
}

void LcdCanvas::switchToPowerOff()
{
    mode = PowerOff;
    clear();
    for (int i = 0; i < (int) (sizeof(groupPowerOff)/sizeof(*groupPowerOff)); i++) {
        groupPowerOff[i]->update();
    }
}

void LcdCanvas::clear()
{
    fillScreen(ST77XX_BLACK);
}

void LcdCanvas::draw()
{
    if (mode == FileView) {
        for (int i = 0; i < (int) (sizeof(groupFileView)/sizeof(*groupFileView)); i++) {
            groupFileView[i]->draw(this);
        }
    } else if (mode == Play) {
        for (int i = 0; i < (int) (sizeof(groupPlay)/sizeof(*groupPlay)); i++) {
            groupPlay[i]->draw(this);
        }
        if (play_count % play_cycle < play_change || !albumArt.isLoaded()) {
            for (int i = 0; i < (int) (sizeof(groupPlay0)/sizeof(*groupPlay0)); i++) {
                groupPlay0[i]->draw(this);
            }
            if (play_count % play_cycle == play_change-1 && albumArt.isLoaded()) {
                for (int i = 0; i < (int) (sizeof(groupPlay0)/sizeof(*groupPlay0)); i++) {
                    groupPlay0[i]->clear(this);
                }
                for (int i = 0; i < (int) (sizeof(groupPlay1)/sizeof(*groupPlay1)); i++) {
                    groupPlay1[i]->update();
                }
            }
        } else {
            for (int i = 0; i < (int) (sizeof(groupPlay1)/sizeof(*groupPlay1)); i++) {
                groupPlay1[i]->draw(this);
            }
            if (play_count % play_cycle == play_cycle-1) {
                for (int i = 0; i < (int) (sizeof(groupPlay1)/sizeof(*groupPlay1)); i++) {
                    groupPlay1[i]->clear(this);
                }
                for (int i = 0; i < (int) (sizeof(groupPlay0)/sizeof(*groupPlay0)); i++) {
                    groupPlay0[i]->update();
                }
            }
        }
        play_count++;
    } else if (mode == PowerOff) {
        for (int i = 0; i < (int) (sizeof(groupPowerOff)/sizeof(*groupPowerOff)); i++) {
            groupPowerOff[i]->draw(this);
        }
    }
}

void LcdCanvas::setFileItem(int column, const char *str, bool isDir, bool isFocused, encoding_t encoding)
{
    uint8_t *icon[2] = {ICON16x16_FILE, ICON16x16_FOLDER};
    uint16_t color[2] = {ST77XX_GRAY, ST77XX_GBLUE};
    fileItem[column].setIcon(icon[isDir]);
    fileItem[column].setFgColor(color[isFocused]);
    fileItem[column].setText(str, encoding);
    fileItem[column].setScroll(isFocused); // Scroll for focused item only
}

void LcdCanvas::setVolume(uint8_t value)
{
    volume.setInt((int) value);
}

void LcdCanvas::setBitRate(uint16_t value)
{
    bitRate.setFormatText("%dKbps", (int) value);
}

void LcdCanvas::setPlayTime(uint32_t positionSec, uint32_t lengthSec)
{
    playTime.setFormatText("%lu:%02lu / %lu:%02lu", positionSec/60, positionSec%60, lengthSec/60, lengthSec%60);
}

void LcdCanvas::setTitle(const char *str, encoding_t encoding)
{
    title.setText(str, encoding);
}

void LcdCanvas::setAlbum(const char *str, encoding_t encoding)
{
    album.setText(str, encoding);
}

void LcdCanvas::setArtist(const char *str, encoding_t encoding)
{
    artist.setText(str, encoding);
}

void LcdCanvas::setAlbumArtJpeg(char *ptr, size_t size)
{
    albumArt.loadJpegBin(ptr, size);
}

void LcdCanvas::setAlbumArtJpeg(FsBaseFile *file, uint64_t pos, size_t size)
{
    albumArt.loadJpegFile(file, pos, size);
}

void LcdCanvas::resetAlbumArt()
{
    albumArt.unload();
}
