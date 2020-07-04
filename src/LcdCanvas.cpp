#include "LcdCanvas.h"
#include "iconfont.h"

//=================================
// Implementation of IconBox class
//=================================
void IconBox::draw(Adafruit_ST7735 *tft) {
    if (!isUpdated) { return; }
    isUpdated = false;
    clear(tft);
    if (icon != NULL) {
        tft->drawBitmap(pos_x, pos_y, icon, 16, 16, fgColor);
    }
}

void IconBox::clear(Adafruit_ST7735 *tft) {
    tft->fillRect(pos_x, pos_y, 16, 16, bgColor); // clear Icon rectangle
}

//=================================
// Implementation of TextBox class
//=================================
void TextBox::setText(const char *str) {
    if (strncmp(this->str, str, size) == 0) { return; }
    isUpdated = true;
    strncpy(this->str, str, size);
}

void TextBox::setFormatText(const char *fmt, ...) {
    char str_temp[size];
    va_list va;
    va_start(va, fmt);
    vsprintf(str_temp, fmt, va);
    va_end(va);
    setText(str_temp);
}

void TextBox::setInt(int value) {
    setFormatText("%d", value);
}

void TextBox::draw(Adafruit_ST7735 *tft) {
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

//=================================
// Implementation of IconTextBox class
//=================================
void IconTextBox::draw(Adafruit_ST7735 *tft) { 
    // For IconBox: Don't display IconBox if str of TextBox is ""
    if (strlen(str) == 0) {
        if (TextBox::isUpdated) { clear(tft); }
    } else {
        IconBox::draw(tft);
    }
    // For TextBox
    TextBox::draw(tft);
}

//=================================
// Implementation of ScrollTextBox class
//=================================
ScrollTextBox::ScrollTextBox(uint16_t pos_x, uint16_t pos_y, uint16_t bgColor) : Box(pos_x, pos_y, bgColor), fgColor(ST77XX_WHITE), scr_en(false)
{
    canvas = new GFXcanvas1(128, 16);
    canvas->setFont(&Nimbus_Sans_L_Regular_Condensed_12);
    canvas->setTextWrap(false);
    canvas->setTextSize(1);
    canvas->getTextBounds("", 0, 0+TEXT_BASELINE_OFS_Y, &x0, &y0, &w0, &h0); // idle-run because first time fails somehow
}

void ScrollTextBox::setText(const char *str) {
    if (strncmp(this->str, str, size) == 0) { return; }
    isUpdated = true;
    strncpy(this->str, str, size);
    canvas->getTextBounds(str, 0, 0+TEXT_BASELINE_OFS_Y, &x0, &y0, &w0, &h0); // get width (w0)
    x_ofs = 0;
    stay_count = 0;
}

void ScrollTextBox::draw(Adafruit_ST7735 *tft) {
    if (!isUpdated && ((128-pos_x-w0) >= 0 || !scr_en)) { return; }
    if ((128-pos_x-w0) >= 0 || !scr_en) {
        x_ofs = 0;
        stay_count = 0;
    } else {
        if (x_ofs == 0 || x_ofs == (128-pos_x-w0)) { // scroll stay a while at both end
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
    canvas->fillRect(0, 0, 128, 16, bgColor);
    canvas->setCursor(x_ofs, TEXT_BASELINE_OFS_Y);
    canvas->print(str);
    tft->drawBitmap(pos_x, pos_y, canvas->getBuffer(), 128, 16, fgColor, bgColor);
}

//=================================
// Implementation of IconScrollTextBox class
//=================================
void IconScrollTextBox::draw(Adafruit_ST7735 *tft) {
    // For IconBox: Don't display IconBox if str of TextBox is ""
    if (strlen(str) == 0) {
        if (ScrollTextBox::isUpdated) { clear(tft); }
    } else {
        IconBox::draw(tft);
    }
    // For ScrollTextBox
    ScrollTextBox::draw(tft);
}

//=================================
// Implementation of LcdCanvas class
//=================================
LcdCanvas::~LcdCanvas()
{
}

void LcdCanvas::init()
{
    initR(INITR_BLACKTAB);      // Init ST7735S chip, black tab
    setTextWrap(false);
    fillScreen(ST77XX_BLACK);
    //setFont(&FreeMono9pt7b);
    setFont(&Nimbus_Sans_L_Regular_Condensed_12);
    setTextSize(1);

    // FileView parts
    for (int i = 0; i < 10; i++) {
        fileItem[i] = new IconScrollTextBox(16*0, 16*i);
        fileItem[i]->setFgColor(ST77XX_GRAY);
    }
    // Play parts
    battery = new IconBox(16*7, 16*0, ICON16x16_BATTERY);
    battery->setFgColor(ST77XX_GRAY);
    volume = new IconTextBox(16*0, 16*0);
    volume->setFgColor(ST77XX_GRAY);
    volume->setIcon(ICON16x16_VOLUME);
    bitRate = new TextBox(16*4, 16*0, AlignCenter);
    bitRate->setFgColor(ST77XX_GRAY);
    playTime = new TextBox(16*8-1, 16*9, AlignRight);
    playTime->setFgColor(ST77XX_GRAY);
    title = new ScrollTextBox(16*0, 16*5);
    title->setText("he cast-expression argument must be a pointer to a block");
    title->setScroll(true);
}

void LcdCanvas::switchToFileView()
{
    mode = FileView;
    clear();
    for (int i = 0; i < 10; i++) {
        fileItem[i]->update();
    }
}

void LcdCanvas::switchToPlay()
{
    mode = Play;
    clear();
    battery->update();
    volume->update();
    bitRate->update();
    playTime->update();
    title->update();
}

void LcdCanvas::clear()
{
    fillScreen(ST77XX_BLACK);
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
    for (int i = 0; i < 10; i++) {
        fileItem[i]->draw(this);
    }
}

void LcdCanvas::drawPlay()
{
    battery->draw(this);
    volume->draw(this);
    bitRate->draw(this);
    playTime->draw(this);
    title->draw(this);
}

void LcdCanvas::setFileItem(int column, const char *str, bool isDir, bool isFocused)
{
    uint8_t *icon[2] = {ICON16x16_FILE, ICON16x16_FOLDER};
    uint16_t color[2] = {ST77XX_GRAY, ST77XX_GBLUE};
    fileItem[column]->setIcon(icon[isDir]);
    fileItem[column]->setFgColor(color[isFocused]);
    fileItem[column]->setText(str);
    fileItem[column]->setScroll(isFocused); // Scroll for focused item only
}

void LcdCanvas::setVolume(uint8_t value)
{
    volume->setInt((int) value);
}

void LcdCanvas::setBitRate(uint16_t value)
{
    bitRate->setFormatText("%dKbps", (int) value);
}

void LcdCanvas::setPlayTime(uint32_t positionSec, uint32_t lengthSec)
{
    playTime->setFormatText("%lu:%02lu / %lu:%02lu", positionSec/60, positionSec%60, lengthSec/60, lengthSec%60);
}