#include "LcdCanvas.h"
#include "iconfont.h"

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
    tft->fillRect(x0, y0, w0, h0, bgColor); // clear previous rectangle
    tft->setTextColor(fgColor);
    x_ofs = (align == AlignRight) ? -w0 : (align == AlignCenter) ? -w0/2 : 0; // at this point, w0 could be not correct
    tft->getTextBounds(str, pos_x+x_ofs, pos_y, &x0, &y0, &w0, &h0, encoding); // update next smallest rectangle (get correct w0)
    x_ofs = (align == AlignRight) ? -w0 : (align == AlignCenter) ? -w0/2 : 0;
    tft->getTextBounds(str, pos_x+x_ofs, pos_y, &x0, &y0, &w0, &h0, encoding); // update next smallest rectangle (get total correct info)
    tft->setCursor(pos_x+x_ofs, pos_y);
    tft->println(str, encoding);
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
    TextBox::setFgColor(fgColor);
    iconBox.setFgColor(fgColor);
}

void IconTextBox::setBgColor(uint16_t bgColor)
{
    TextBox::setBgColor(bgColor);
    iconBox.setBgColor(bgColor);
}

void IconTextBox::update()
{
    TextBox::update();
    iconBox.update();
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
    ScrollTextBox::setFgColor(fgColor);
    iconBox.setFgColor(fgColor);
}

void IconScrollTextBox::setBgColor(uint16_t bgColor)
{
    ScrollTextBox::setBgColor(bgColor);
    iconBox.setBgColor(bgColor);
}

void IconScrollTextBox::update()
{
    ScrollTextBox::update();
    iconBox.update();
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

void IconScrollTextBox::setIcon(uint8_t *icon)
{
    iconBox.setIcon(icon);
}

//=================================
// Implementation of LcdCanvas class
//=================================
LcdCanvas::LcdCanvas(int8_t cs, int8_t dc, int8_t rst) : Adafruit_ST7735(cs, dc, rst), mode(FileView)
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