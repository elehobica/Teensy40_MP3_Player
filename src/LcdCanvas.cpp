#include "LcdCanvas.h"
#include "iconfont.h"

//=================================
// Implementation of TextBox class
//=================================
void TextBox::setText(const char *str) {
    if (strncmp(this->str, str, 256) == 0) { return; }
    isUpdated = true;
    strncpy(this->str, str, 256);
}

void TextBox::setFormatText(const char *fmt, ...) {
    char str_temp[256];
    va_list va;
    va_start(va, fmt);
    vsprintf(str_temp, fmt, va);
    va_end(va);
    setText(str_temp);
    Serial.println(fmt);
    Serial.println(str_temp);
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
    x_ofs = (align == AlignRight) ? -w0 : 0;
    tft->getTextBounds(str, pos_x+x_ofs, pos_y+TEXT_BASELINE_OFS_Y, &x0, &y0, &w0, &h0); // update next smallest rectangle
    x_ofs = (align == AlignRight) ? -w0 : 0;
    tft->setCursor(pos_x+x_ofs, pos_y+TEXT_BASELINE_OFS_Y);
    tft->println(str);
}

//=================================
// Implementation of IconTextBox class
//=================================
void IconTextBox::draw(Adafruit_ST7735 *tft) {
    if (!isUpdated) { return; }
    tft->fillRect(pos_x-16, pos_y, 16, 16, bgColor); // clear Icon rectangle
    if (icon != NULL && strlen(str) != 0) {
        tft->drawBitmap(pos_x-16, pos_y, icon, 16, 16, fgColor);
    }
    TextBox::draw(tft);
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
        fileItem[i] = new IconTextBox(16*0, 16*i);
        fileItem[i]->setFgColor(ST77XX_GRAY);
    }
    // Play parts
    volume = new IconTextBox(16*0, 16*0);
    volume->setFgColor(ST77XX_GRAY);
    volume->setIcon(ICON16x16_VOLUME);
    bitRate = new TextBox(16*3, 16*0);
    bitRate->setFgColor(ST77XX_GRAY);
    playTime = new TextBox(16*8-1, 16*9, ST77XX_BLACK, AlignRight);
    playTime->setFgColor(ST77XX_GRAY);
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
    drawBitmap(16*7, 16*0, ICON16x16_BATTERY, 16, 16, ST77XX_GRAY);
    volume->update();
    bitRate->update();
    playTime->update();
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
    volume->draw(this);
    bitRate->draw(this);
    playTime->draw(this);
}

void LcdCanvas::setFileItem(int column, const char *str, bool isDir, bool isFocused)
{
    uint8_t *icon[2] = {ICON16x16_FILE, ICON16x16_FOLDER};
    uint16_t color[2] = {ST77XX_GRAY, ST77XX_GBLUE};
    fileItem[column]->setIcon(icon[isDir]);
    fileItem[column]->setFgColor(color[isFocused]);
    fileItem[column]->setText(str);
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