/*-----------------------------------------------------------/
/ LcdCanvas
/------------------------------------------------------------/
/ Copyright (c) 2020, Elehobica
/ Released under the BSD-2-Clause
/ refer to https://opensource.org/licenses/BSD-2-Clause
/-----------------------------------------------------------*/

#include "LcdCanvas.h"
//#include "fonts/iconfont.h"

//=================================
// Implementation of BatteryIconBox class
//=================================
BatteryIconBox::BatteryIconBox(int16_t pos_x, int16_t pos_y, uint16_t fgColor, uint16_t bgColor)
    : IconBox(pos_x, pos_y, ICON16x16_BATTERY, fgColor, bgColor), level(0) {}

void BatteryIconBox::draw(Adafruit_SPITFT *tft)
{
    if (!isUpdated) { return; }
    isUpdated = false;
    clear(tft);
    if (icon != NULL) {
        tft->drawBitmap(pos_x, pos_y, icon, iconWidth, iconHeight, fgColor);
        uint16_t color = (level >= 50) ? 0x0600 : (level >= 20) ? 0xc600 : 0xc000;
        if (level/10 < 9) {
            tft->fillRect(pos_x+4, pos_y+4, 8, 10-level/10-1, bgColor);
        }
        tft->fillRect(pos_x+4, pos_y+13-level/10, 8, level/10+1, color);
    }
}

void BatteryIconBox::setLevel(uint8_t value)
{
    value = (value <= 100) ? value : 100;
    if (this->level == value) { return; }
    this->level = value;
    update();
}

//=================================
// Implementation of LcdCanvas class
//=================================
LcdCanvas::LcdCanvas(int8_t cs, int8_t dc, int8_t rst) : Adafruit_LCD(cs, dc, rst), play_count(0)
{
    #ifdef USE_ST7735_128x160
    initR(INITR_BLACKTAB, SPI_FREQ);      // Init ST7735S chip, black tab
    #endif
    #ifdef USE_ST7789_240x240_WOCS
    init(240, 240, SPI_MODE2, SPI_FREQ);
    #endif
    #ifdef USE_ILI9341_240x320
    Adafruit_LCD::begin(SPI_FREQ); // this could be overclock of SPI clock, but works
    #endif
    setTextWrap(false);
    fillScreen(LCD_BLACK);
    setFont(CUSTOM_FONT, CUSTOM_FONT_OFS_Y);
    setTextSize(1);

    // ListView parts (nothing to set here)

    // Play parts (nothing to set here)
}

LcdCanvas::~LcdCanvas()
{
}

void LcdCanvas::switchToListView()
{
    clear();
    msg.setText("");
    for (int i = 0; i < (int) (sizeof(groupListView)/sizeof(*groupListView)); i++) {
        groupListView[i]->update();
    }
}

void LcdCanvas::switchToPlay()
{
    clear();
    msg.setText("");
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

void LcdCanvas::switchToPowerOff(const char *msg_str)
{
    clear();
    msg.setText("");
    if (msg_str != NULL) { msg.setText(msg_str); }
    for (int i = 0; i < (int) (sizeof(groupPowerOff)/sizeof(*groupPowerOff)); i++) {
        groupPowerOff[i]->update();
    }
}

void LcdCanvas::clear()
{
    fillScreen(LCD_BLACK);
}

void LcdCanvas::drawListView()
{
    for (int i = 0; i < (int) (sizeof(groupListView)/sizeof(*groupListView)); i++) {
        groupListView[i]->draw(this);
    }
}

void LcdCanvas::drawPlay()
{
    for (int i = 0; i < (int) (sizeof(groupPlay)/sizeof(*groupPlay)); i++) {
        groupPlay[i]->draw(this);
    }
    if (play_count % play_cycle < play_change || albumArt.getCount() == 0) { // Play mode 0 display
        for (int i = 0; i < (int) (sizeof(groupPlay0)/sizeof(*groupPlay0)); i++) {
            groupPlay0[i]->draw(this);
        }
        if (albumArt.getCount() == 0) {
            msg.setText("No Image");
        } else if (play_count == 0 && albumArt.getCount() > 0) {
            if (albumArt.loadNext()) {
                msg.setText("");
            } else {
                msg.setText("Not Supported Image");
            }
            #ifdef USE_ALBUM_ART_SMALL
            if (albumArtSmall.loadNext()) {
                msg.setText("");
            } else {
                msg.setText("Not Supported Image");
            }
            #endif // #ifdef USE_ALBUM_ART_SMALL
        } else if (play_count % play_cycle == play_change-1 && albumArt.getCount() > 0) { // Play mode 0 -> 1
            for (int i = 0; i < (int) (sizeof(groupPlay)/sizeof(*groupPlay)); i++) {
                groupPlay[i]->update();
            }
            for (int i = 0; i < (int) (sizeof(groupPlay0)/sizeof(*groupPlay0)); i++) {
                groupPlay0[i]->clear(this);
            }
            for (int i = 0; i < (int) (sizeof(groupPlay1)/sizeof(*groupPlay1)); i++) {
                groupPlay1[i]->update();
            }
            if (albumArt.getCount() > 1) {
                albumArt.clear(this);
                if (albumArt.loadNext()) {
                    msg.setText("");
                } else {
                    msg.setText("Not Supported Image");
                }
            }
        }
    } else { // Play mode 1 display
        for (int i = 0; i < (int) (sizeof(groupPlay1)/sizeof(*groupPlay1)); i++) {
            groupPlay1[i]->draw(this);
        }
        if (play_count % play_cycle == play_cycle-1) { // Play mode 1 -> 0
            for (int i = 0; i < (int) (sizeof(groupPlay)/sizeof(*groupPlay)); i++) {
                groupPlay[i]->update();
            }
            for (int i = 0; i < (int) (sizeof(groupPlay1)/sizeof(*groupPlay1)); i++) {
                groupPlay1[i]->clear(this);
            }
            for (int i = 0; i < (int) (sizeof(groupPlay0)/sizeof(*groupPlay0)); i++) {
                groupPlay0[i]->update();
            }
        }
    }
    play_count++;
}

void LcdCanvas::drawPowerOff()
{
    for (int i = 0; i < (int) (sizeof(groupPowerOff)/sizeof(*groupPowerOff)); i++) {
        groupPowerOff[i]->draw(this);
    }
}

void LcdCanvas::setListItem(int column, const char *str, const uint8_t *icon, bool isFocused, encoding_t encoding)
{
    uint16_t color[2] = {LCD_GRAY, LCD_GBLUE};
    listItem[column].setIcon(icon);
    listItem[column].setFgColor(color[isFocused]);
    listItem[column].setText(str, encoding);
    listItem[column].setScroll(isFocused); // Scroll for focused item only
}

void LcdCanvas::setBitRate(uint16_t value)
{
    if (value > 0) {
        bitRate.setFormatText("%dKbps", (int) value);
    } else {
        bitRate.setText("");
    }
}

void LcdCanvas::setBitResolution(uint16_t value)
{
    switch(value) {
        case 16: memcpy(&bitSampIcon[0], ICON16x16_16BIT, 16); break;
        case 24: memcpy(&bitSampIcon[0], ICON16x16_24BIT, 16); break;
        case 32: memcpy(&bitSampIcon[0], ICON16x16_32BIT, 16); break;
        default: memset(&bitSampIcon[0], 0, 16); break;
    }
    bitSamp.setIcon(bitSampIcon);
}

void LcdCanvas::setSampleFreq(uint32_t sampFreq)
{
    switch(sampFreq) {
        case 44100:  memcpy(&bitSampIcon[16], ICON16x16_44_1KHZ  + 16, 16); break;
        case 48000:  memcpy(&bitSampIcon[16], ICON16x16_48_0KHZ  + 16, 16); break;
        case 88200:  memcpy(&bitSampIcon[16], ICON16x16_88_2KHZ  + 16, 16); break;
        case 96000:  memcpy(&bitSampIcon[16], ICON16x16_96_0KHZ  + 16, 16); break;
        case 176400: memcpy(&bitSampIcon[16], ICON16x16_176_4KHZ + 16, 16); break;
        case 192000: memcpy(&bitSampIcon[16], ICON16x16_192_0KHZ + 16, 16); break;
        default: memset(&bitSampIcon[16], 0, 16); break;
    }
    bitSamp.setIcon(bitSampIcon);
}

void LcdCanvas::setVolume(uint8_t value)
{
    volume.setInt((int) value);
}

void LcdCanvas::setTrack(const char *str)
{
    if (strlen(str)) {
        track.setFormatText("[ %s ]", str);
    } else {
        track.setText("");
    }
}

void LcdCanvas::setPlayTime(uint32_t positionSec, uint32_t lengthSec, bool blink)
{
    playTime.setFormatText("%lu:%02lu / %lu:%02lu", positionSec/60, positionSec%60, lengthSec/60, lengthSec%60);
    playTime.setBlink(blink);
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

void LcdCanvas::setYear(const char *str, encoding_t encoding)
{
    year.setText(str, encoding);
}

void LcdCanvas::setBatteryVoltage(uint16_t voltage_x1000)
{
    const uint16_t lvl100 = 4100;
    const uint16_t lvl0 = 2900;
    battery.setLevel(((voltage_x1000 - lvl0) * 100) / (lvl100 - lvl0));
}

void LcdCanvas::addAlbumArtJpeg(uint16_t file_idx, uint64_t pos, size_t size, bool is_unsync)
{
    albumArt.addJpegFile(file_idx, pos, size, is_unsync);
    #ifdef USE_ALBUM_ART_SMALL
    albumArtSmall.addJpegFile(file_idx, pos, size, is_unsync);
    #endif // #ifdef USE_ALBUM_ART_SMALL
}

void LcdCanvas::addAlbumArtPng(uint16_t file_idx, uint64_t pos, size_t size, bool is_unsync)
{
    albumArt.addPngFile(file_idx, pos, size, is_unsync);
    #ifdef USE_ALBUM_ART_SMALL
    albumArtSmall.addPngFile(file_idx, pos, size, is_unsync);
    #endif // #ifdef USE_ALBUM_ART_SMALL
}

void LcdCanvas::deleteAlbumArt()
{
    albumArt.deleteAll();
    #ifdef USE_ALBUM_ART_SMALL
    albumArtSmall.deleteAll();
    #endif // #ifdef USE_ALBUM_ART_SMALL
}
