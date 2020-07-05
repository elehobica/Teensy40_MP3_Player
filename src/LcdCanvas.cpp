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
    : isUpdated(true), pos_x(pos_x), pos_y(pos_y), fgColor(fgColor), bgColor(bgColor), align(AlignLeft), str("") {}

TextBox::TextBox(int16_t pos_x, int16_t pos_y, align_enm align, uint16_t fgColor, uint16_t bgColor)
    : isUpdated(true), pos_x(pos_x), pos_y(pos_y), fgColor(fgColor), bgColor(bgColor), align(align), str("") {}

TextBox::TextBox(int16_t pos_x, int16_t pos_y, const char *str, align_enm align, uint16_t fgColor, uint16_t bgColor)
    : isUpdated(true), pos_x(pos_x), pos_y(pos_y), fgColor(fgColor), bgColor(bgColor), align(align), str("")
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
    tft->getTextBounds(str, pos_x+x_ofs, pos_y+TEXT_BASELINE_OFS_Y, &x0, &y0, &w0, &h0); // update next smallest rectangle (get correct w0)
    x_ofs = (align == AlignRight) ? -w0 : (align == AlignCenter) ? -w0/2 : 0;
    tft->getTextBounds(str, pos_x+x_ofs, pos_y+TEXT_BASELINE_OFS_Y, &x0, &y0, &w0, &h0); // update next smallest rectangle (get total correct info)
    tft->setCursor(pos_x+x_ofs, pos_y+TEXT_BASELINE_OFS_Y);
    tft->println(str);
}

void TextBox::setText(const char *str)
{
    if (strncmp(this->str, str, charSize) == 0) { return; }
    strncpy(this->str, str, charSize);
    update();
}

void TextBox::setFormatText(const char *fmt, ...)
{
    char str_temp[charSize];
    va_list va;
    va_start(va, fmt);
    vsprintf(str_temp, fmt, va);
    va_end(va);
    setText(str_temp);
}

void TextBox::setInt(int value)
{
    setFormatText("%d", value);
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
    : isUpdated(true), pos_x(pos_x), pos_y(pos_y), fgColor(fgColor), bgColor(bgColor), str(""), width(width), stay_count(0), scr_en(true)
{
    int16_t x0, y0; // dummy
    uint16_t h0; // dummy
    canvas = new GFXcanvas1(this->width, FONT_HEIGHT);
    canvas->setFont(DEFAULT_FONT);
    canvas->setTextWrap(false);
    canvas->setTextSize(1);
    canvas->getTextBounds("", 0, 0+TEXT_BASELINE_OFS_Y, &x0, &y0, &w0, &h0); // idle-run because first time fails somehow
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
                if (x_ofs-- != 0) { x_ofs = 0; }
                stay_count = 0;
            }
        } else {
            x_ofs--;
        }
    }
    if (!isUpdated && stay_count != 0) { return; }
    isUpdated = false;
    // Flicker less draw (width must be ScrollTextBox's width)
    canvas->fillRect(0, 0, width, FONT_HEIGHT, bgColor);
    canvas->setCursor(x_ofs, TEXT_BASELINE_OFS_Y);
    canvas->print(str);
    tft->drawBitmap(pos_x, pos_y, canvas->getBuffer(), width, FONT_HEIGHT, fgColor, bgColor);
}

void ScrollTextBox::setScroll(bool scr_en)
{
    if (this->scr_en == scr_en) { return; }
    this->scr_en = scr_en;
    update();
}

void ScrollTextBox::setText(const char *str)
{
    int16_t x0, y0; // dummy
    uint16_t h0; // dummy
    if (strncmp(this->str, str, charSize) == 0) { return; }
    update();
    strncpy(this->str, str, charSize);
    canvas->getTextBounds(str, 0, 0+TEXT_BASELINE_OFS_Y, &x0, &y0, &w0, &h0); // get width (w0)
    x_ofs = 0;
    stay_count = 0;
}

//=================================
// Implementation of IconScrollTextBox class < ScrollTextBox
//=================================
IconScrollTextBox::IconScrollTextBox(int16_t pos_x, int16_t pos_y, uint16_t width, uint16_t fgColor, uint16_t bgColor)
    : ScrollTextBox(pos_x+16, pos_y, width-16, fgColor, bgColor), iconBox(pos_x, pos_y, fgColor, bgColor) {}

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
    //setFont(&FreeMono9pt7b);
    setFont(DEFAULT_FONT);
    setTextSize(1);
    //Serial.println(width());
    //Serial.println(height());

    // FileView parts

    // Play parts
    title.setText("he cast-expression argument must be a pointer to a block");
    artist.setText("error: expected identifier before numeric constant");
    album.setText("alakekeeeeeeeeeeeeeeeeeeeeeeeeeeee133");
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

void LcdCanvas::clear()
{
    fillScreen(ST77XX_BLACK);
}

void LcdCanvas::bye()
{
    mode = PowerOff;
    clear();
    TextBox bye_msg = TextBox(width()/2, height()/2-FONT_HEIGHT+TEXT_BASELINE_OFS_Y, "Bye", AlignCenter);
    bye_msg.draw(this);
}

void LcdCanvas::draw()
{
    if (mode == FileView) {
        drawFileView();
    } else if (mode == Play) {
        drawPlay();
    }
}

void LcdCanvas::drawFileView()
{
    for (int i = 0; i < (int) (sizeof(groupFileView)/sizeof(*groupFileView)); i++) {
        groupFileView[i]->draw(this);
    }
}

void LcdCanvas::drawPlay()
{
    for (int i = 0; i < (int) (sizeof(groupPlay)/sizeof(*groupPlay)); i++) {
        groupPlay[i]->draw(this);
    }
}

void LcdCanvas::setFileItem(int column, const char *str, bool isDir, bool isFocused)
{
    uint8_t *icon[2] = {ICON16x16_FILE, ICON16x16_FOLDER};
    uint16_t color[2] = {ST77XX_GRAY, ST77XX_GBLUE};
    fileItem[column].setIcon(icon[isDir]);
    fileItem[column].setFgColor(color[isFocused]);
    fileItem[column].setText(str);
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