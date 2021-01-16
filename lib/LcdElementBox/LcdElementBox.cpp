/*-----------------------------------------------------------/
/ LcdElementBox: Lcd Element Box API for Adafruit_SPITFT v0.90
/------------------------------------------------------------/
/ Copyright (c) 2020, Elehobica
/ Released under the BSD-2-Clause
/ refer to https://opensource.org/licenses/BSD-2-Clause
/-----------------------------------------------------------*/

#include "LcdElementBox.h"
#include <JPEGDecoder.h>
#include <PNGDecoder.h>
#include <file_menu_SdFat.h>

//=================================
// Implementation of ImageBox class
//=================================
ImageBox::ImageBox(int16_t pos_x, int16_t pos_y, uint16_t width, uint16_t height, uint16_t bgColor)
    : isUpdated(true), pos_x(pos_x), pos_y(pos_y), width(width), height(height), bgColor(bgColor),
        decode_ok(true), image(NULL), img_w(0), img_h(0), src_w(0), src_h(0), ratio256_w(256), ratio256_h(256),
        isLoaded(false), changeNext(true), resizeFit(true), keepAspectRatio(true), align(center),
        image_count(0), image_idx(0)
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

void ImageBox::draw(Adafruit_SPITFT *tft)
{
    if (!isUpdated || image == NULL || !isLoaded) { return; }
    isUpdated = false;
    int16_t ofs_x = 0;
    int16_t ofs_y = 0;

    if (align == center) {
        ofs_x = (width-img_w)/2;
        ofs_y = (height-img_h)/2;
    }
    tft->drawRGBBitmap(pos_x+ofs_x, pos_y+ofs_y, image, img_w, img_h);
}

void ImageBox::clear(Adafruit_SPITFT *tft)
{
    tft->fillRect(pos_x, pos_y, width, height, bgColor); // clear Icon rectangle
}

void ImageBox::setResizeFit(bool flg)
{
    resizeFit = flg;
}

void ImageBox::setKeepAspectRatio(bool flg)
{
    keepAspectRatio = flg;
}

void ImageBox::setImageBuf(int16_t x, int16_t y, uint16_t rgb565)
{
    if (image == NULL) { return; }
    if (!(x >= 0 && x < width && y >= 0 && y < height)) { return; }
    image[width*y+x] = rgb565;
    if (x+1 > img_w) { img_w = x+1; }
    if (y+1 > img_h) { img_h = y+1; }
}

// add JPEG binary
int ImageBox::addJpegBin(char *ptr, size_t size)
{
    if (image_count >= MaxImgCnt) { return 0; }
    image_array[image_count].media_src = char_ptr;
    image_array[image_count].img_fmt = jpeg;
    image_array[image_count].ptr = ptr;
    image_array[image_count].file_idx = 0;
    image_array[image_count].file_pos = 0;
    image_array[image_count].size = size;
    image_array[image_count].is_unsync = false;
    image_count++;
    return 1;
}

// add JPEG SdFat File
int ImageBox::addJpegFile(uint16_t file_idx, uint64_t pos, size_t size, bool is_unsync)
{
    if (image_count >= MaxImgCnt) { return 0; }
    image_array[image_count].media_src = sdcard;
    image_array[image_count].img_fmt = jpeg;
    image_array[image_count].ptr = NULL;
    image_array[image_count].file_idx = file_idx;
    image_array[image_count].file_pos = pos;
    image_array[image_count].size = size;
    image_array[image_count].is_unsync = is_unsync;
    image_count++;
    return 1;
}

// add PNG binary
int ImageBox::addPngBin(char *ptr, size_t size)
{
    if (image_count >= MaxImgCnt) { return 0; }
    image_array[image_count].media_src = char_ptr;
    image_array[image_count].img_fmt = png;
    image_array[image_count].ptr = ptr;
    image_array[image_count].file_idx = 0;
    image_array[image_count].file_pos = 0;
    image_array[image_count].size = size;
    image_array[image_count].is_unsync = false;
    image_count++;
    return 1;
}

// add PNG SdFat File
int ImageBox::addPngFile(uint16_t file_idx, uint64_t pos, size_t size, bool is_unsync)
{
    if (image_count >= MaxImgCnt) { return 0; }
    image_array[image_count].media_src = sdcard;
    image_array[image_count].img_fmt = png;
    image_array[image_count].ptr = NULL;
    image_array[image_count].file_idx = file_idx;
    image_array[image_count].file_pos = pos;
    image_array[image_count].size = size;
    image_array[image_count].is_unsync = is_unsync;
    image_count++;
    return 1;
}

bool ImageBox::loadNext()
{
    // In a single image case, no need to reload
    if (image_count <= 0 || !changeNext) { return decode_ok; }

    int image_idx_next;
    unload();
    if (image_array[image_idx].img_fmt == jpeg) {
        if (image_array[image_idx].media_src == char_ptr) {
            decode_ok = !loadJpegBin(image_array[image_idx].ptr, image_array[image_idx].size);
        } else if (image_array[image_idx].media_src == sdcard) {
            decode_ok = loadJpegFile(image_array[image_idx].file_idx, image_array[image_idx].file_pos, image_array[image_idx].size, image_array[image_idx].is_unsync);
        }
    } else if (image_array[image_idx].img_fmt == png) {
        if (image_array[image_idx].media_src == char_ptr) {
            decode_ok = loadPngBin(image_array[image_idx].ptr, image_array[image_idx].size);
        } else if (image_array[image_idx].media_src == sdcard) {
            decode_ok = loadPngFile(image_array[image_idx].file_idx, image_array[image_idx].file_pos, image_array[image_idx].size, image_array[image_idx].is_unsync);
        }
    }
    image_idx_next = (image_idx + 1) % image_count;
    if (image_idx_next == image_idx) { // == 0
        changeNext = false;
    }
    image_idx = image_idx_next;
    return decode_ok;
}

// ==================================================================================
// JPEG Resizing Policy
// Step 1. use reduce mode of picojpeg if image is 8x larger (1/1, 1/8)
// Step 2. use MCU unit accumulation if image is 2x larger (1/1, 1/2, 1/4, 1/8, 1/16)
// Step 3. fit to width/height by Nearest Neighbor (shrink or expand)
// ==================================================================================

// load from JPEG binary
bool ImageBox::loadJpegBin(char *ptr, size_t size)
{
    int decoded;
    bool reduce = false;
    JpegDec.abort();
    decoded = JpegDec.decodeArray((const uint8_t *) ptr, (uint32_t) size, 0); // reduce == 0
    if (decoded <= 0) { return false; }
    src_w = JpegDec.width;
    src_h = JpegDec.height;
    if (resizeFit && (
        (keepAspectRatio && (src_w >= width*8 && src_h >= height*8)) ||
        (!keepAspectRatio && (src_w >= width*8 || src_h >= height*8))
    )) { // Use reduce decode for x8 larger image
        reduce = true;
        JpegDec.abort();
        decoded = JpegDec.decodeArray((const uint8_t *) ptr, (uint32_t) size, 1); // reduce == 1
        if (decoded <= 0) { return false; }
    }
    loadJpeg(reduce);
    return true;
}

// load from JPEG File
bool ImageBox::loadJpegFile(uint16_t file_idx, uint64_t pos, size_t size, bool is_unsync)
{
    int decoded;
    bool reduce = false;
    JpegDec.abort();
    file_menu_get_obj(file_idx, &file);
    decoded = JpegDec.decodeSdFile(&file, pos, size, 0, is_unsync); // reduce == 0
    if (decoded <= 0) { return false; }
    src_w = JpegDec.width;
    src_h = JpegDec.height;
    if (resizeFit && (
        (keepAspectRatio && (src_w >= width*8 && src_h >= height*8)) ||
        (!keepAspectRatio && (src_w >= width*8 || src_h >= height*8))
    )) { // Use reduce decode for x8 larger image
        reduce = true;
        JpegDec.abort();
        file_menu_get_obj(file_idx, &file);
        decoded = JpegDec.decodeSdFile(&file, pos, size, 1, is_unsync); // reduce == 1
        if (decoded <= 0) { return false; }
    }
    loadJpeg(reduce);
    return true;
}

// MCU block 1/2 Accumulation (Shrink) x count times
void ImageBox::jpegMcu2sAccum(int count, uint16_t mcu_w, uint16_t mcu_h, uint16_t *pImage)
{
    int i;
    mcu_w <<= count;
    mcu_h <<= count;
    for (i = 0; i < count; i++) {
        for (int16_t mcu_ofs_y = 0; mcu_ofs_y < mcu_h; mcu_ofs_y+=2) {
            for (int16_t mcu_ofs_x = 0; mcu_ofs_x < mcu_w; mcu_ofs_x+=2) {
                uint32_t r = 0;
                uint32_t g = 0;
                uint32_t b = 0;
                for (int y = 0; y < 2; y++) {
                    for (int x = 0; x < 2; x++) {
                        // RGB565 format
                        r += pImage[mcu_w*(mcu_ofs_y+y)+mcu_ofs_x+x] & 0xf800;
                        g += pImage[mcu_w*(mcu_ofs_y+y)+mcu_ofs_x+x] & 0x07e0;
                        b += pImage[mcu_w*(mcu_ofs_y+y)+mcu_ofs_x+x] & 0x001f;
                    }
                }
                r = (r / 4) & 0xf800;
                g = (g / 4) & 0x07e0;
                b = (b / 4) & 0x001f;
                pImage[mcu_w/2*(mcu_ofs_y/2)+mcu_ofs_x/2] = ((uint16_t) r | (uint16_t) g | (uint16_t) b);
            }
        }
        mcu_w /= 2;
        mcu_h /= 2;
    }
}

void ImageBox::loadJpeg(bool reduce)
{
    if (image == NULL) { return; }
    src_w = JpegDec.width;
    src_h = JpegDec.height;
    uint16_t mcu_w = JpegDec.MCUWidth;
    uint16_t mcu_h = JpegDec.MCUHeight;
    #ifdef DEBUG_LCD_ELEMENT_BOX
    { // DEBUG
        char str[256];
        sprintf(str, "JPEG info: (w, h) = (%d, %d), (mcu_w, mcu_h) = (%d, %d)", src_w, src_h, mcu_w, mcu_h);
        Serial.println(str);
    }
    #endif // DEBUG_LCD_ELEMENT_BOX
   if (reduce) {
        src_w /= 8;
        src_h /= 8;
        mcu_w /= 8;
        mcu_h /= 8;
        #ifdef DEBUG_LCD_ELEMENT_BOX
        { // DEBUG
            char str[256];
            sprintf(str, "Reduce applied:  (w, h) = (%d, %d), (virtual) (mcu_w, mcu_h) = (%d, %d)", src_w, src_h, mcu_w, mcu_h);
            Serial.println(str);
        }
        #endif // DEBUG_LCD_ELEMENT_BOX
   }
    // Calculate MCU 2's Accumulation Count
    int mcu_2s_accum_cnt = 0;
    {
        while (1) {
            if (!resizeFit) break;
            if (keepAspectRatio) {
                if (src_w <= width * 2 && src_h <= height * 2) break;
            } else {
                if (src_w <= width * 2 || src_h <= height * 2) break;
            }
            if (mcu_w == 1 || mcu_h == 1) break;
            src_w /= 2;
            src_h /= 2;
            mcu_w /= 2;
            mcu_h /= 2;
            mcu_2s_accum_cnt++;
        }
        #ifdef DEBUG_LCD_ELEMENT_BOX
        { // DEBUG
            if (mcu_2s_accum_cnt > 0) {
                char str[256];
                sprintf(str, "Accumulated %d times: (w, h) = (%d, %d), (mcu_w, mcu_h) = (%d, %d)", mcu_2s_accum_cnt, src_w, src_h, mcu_w, mcu_h);
                Serial.println(str);
            }
        }
        #endif // DEBUG_LCD_ELEMENT_BOX
    }

    int16_t mcu_y_prev = 0;
    int16_t mod_y_start = 0;
    int16_t plot_y_start = 0;
    while (JpegDec.read()) {
        // MCU 2's Accumulation
        jpegMcu2sAccum(mcu_2s_accum_cnt, mcu_w, mcu_h, JpegDec.pImage);
        int idx = 0;
        int16_t x, y;
        int16_t mcu_x = JpegDec.MCUx;
        int16_t mcu_y = JpegDec.MCUy;
        int16_t mod_y_pls = (!keepAspectRatio || src_h >= src_w) ? src_h : src_w;
        int16_t mod_y = mod_y_pls;
        int16_t plot_y = 0;
        // prepare plot_y (, mod_y) condition
        if (!resizeFit) {
            plot_y = mcu_h * mcu_y;
            if (plot_y >= height) continue; // don't use break because MCU order is not always left-to-right and top-to-bottom
        } else {
            if (mcu_y != mcu_y_prev) {
                for (y = 0; y < mcu_h * mcu_y; y++) {
                    while (mod_y < 0) {
                        mod_y += mod_y_pls;
                        plot_y++;
                    }
                    mod_y -= height;
                }
                // memorize plot_y (, mod_y) start condition
                mcu_y_prev = mcu_y;
                mod_y_start = mod_y;
                plot_y_start = plot_y;
            } else {
                // reuse plot_y (, mod_y) start condition
                mod_y = mod_y_start;
                plot_y = plot_y_start;
            }
        }
        int16_t mod_x_start = 0;
        int16_t plot_x_start = 0;
        for (int16_t mcu_ofs_y = 0; mcu_ofs_y < mcu_h; mcu_ofs_y++) {
            y = mcu_h * mcu_y + mcu_ofs_y;
            if (y >= src_h) break;
            int16_t mod_x_pls = (!keepAspectRatio || src_w >= src_h) ? src_w : src_h;
            int16_t mod_x = mod_x_pls;
            int16_t plot_x = 0;
            // prepare plot_x (, mod_x) condition
            if (!resizeFit) {
                plot_x = mcu_w * mcu_x;
                if (plot_x >= width) break;
            } else {
                if (mcu_ofs_y == 0) {
                    for (x = 0; x < mcu_w * mcu_x; x++) {
                        while (mod_x < 0) {
                            mod_x += mod_x_pls;
                            plot_x++;
                        }
                        mod_x -= width;
                    }
                    // memorize plot_x (, mod_x) start condition
                    mod_x_start = mod_x;
                    plot_x_start = plot_x;
                } else {
                    // reuse plot_x (, mod_x) start condition
                    mod_x = mod_x_start;
                    plot_x = plot_x_start;
                }
            }
            // actual plot_x
            for (int16_t mcu_ofs_x = 0; mcu_ofs_x < mcu_w; mcu_ofs_x++) {
                x = mcu_w * mcu_x + mcu_ofs_x;
                if (x >= src_w) {
                    idx += mcu_w - src_w%mcu_w; // skip horizontal padding area
                    break;
                }
                if (!resizeFit) {
                    if (plot_x >= width) break;
                    image[width*plot_y+plot_x] = JpegDec.pImage[idx];
                    if (plot_x+1 > img_w) { img_w = plot_x+1; }
                    plot_x++;
                } else {
                    if (mod_y < 0) {
                        while (mod_x < 0) {
                            if (plot_x >= width) break;
                            image[width*plot_y+plot_x] = JpegDec.pImage[idx];
                            if (plot_x+1 > img_w) { img_w = plot_x+1; }
                            mod_x += mod_x_pls;
                            plot_x++;
                        }
                        mod_x -= width;
                    }
                }
                idx++;
            }
            if (!resizeFit) {
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
                        for (x = plot_x_start; x < plot_x; x++) {
                            image[width*plot_y+x] = image[width*(plot_y-1)+x];
                        }
                    }
                }
                mod_y -= height;
            }
        }
    }
    #ifdef DEBUG_LCD_ELEMENT_BOX
    { // DEBUG
        char str[256];
        sprintf(str, "Resized to (img_w, img_h) = (%d, %d)", img_w, img_h);
        Serial.println(str);
    }
    #endif // DEBUG_LCD_ELEMENT_BOX
    if (img_w < width) { // delete horizontal blank
        for (int16_t plot_y = 1; plot_y < img_h; plot_y++) {
            memmove(&image[img_w*plot_y], &image[width*plot_y], img_w*2);
        }
    }
    isLoaded = true;
    update();
}

// ==================================================================================
// PNG Resizing Policy
// Step 1. use reduce mode added to pngle if image is more than 2x larger (1/2, 1/4, ... )
// Step 2. fit to width/height by Nearest Neighbor (shrink or expand)
// (Step 3). in expand case, copy pixel to nearby pixels
// ==================================================================================

void cb_pngdec_draw_with_resize(void *cb_obj, uint32_t x, uint32_t y, uint16_t rgb565)
{
    ImageBox *ib = (ImageBox *) cb_obj;

    if (ib->image == NULL) { return; }
    if (ib->resizeFit) {
        x = x * ib->ratio256_w / 256;
        y = y * ib->ratio256_h / 256;
    }
    if (!(x >= 0 && x < ib->width && y >= 0 && y < ib->height)) { return; }
    for (int16_t ofs_y = 0; ofs_y < (int16_t) ((ib->ratio256_h+255)/256); ofs_y++) {
        int16_t plot_y = y + ofs_y;
        if (plot_y >= ib->height) { break; }
        for (int16_t ofs_x = 0; ofs_x < (int16_t) ((ib->ratio256_w+255)/256); ofs_x++) {
            int16_t plot_x = x + ofs_x;
            if (plot_x >= ib->width) { continue; }
            ib->setImageBuf(plot_x, plot_y, rgb565);
        }
    }
}

// load from PNG binary (Not verified yet)
bool ImageBox::loadPngBin(char *ptr, size_t size)
{
    int decoded;
    uint8_t reduce = 0;
    PngDec.abort();
    decoded = PngDec.loadArray((const uint8_t *) ptr, (uint32_t) size);
    if (decoded <= 0) { return false; }
    src_w = PngDec.width;
    src_h = PngDec.height;
    while (resizeFit && (
        (keepAspectRatio && (src_w > width*2 && src_h > height*2)) ||
        (!keepAspectRatio && (src_w > width*2 || src_h > height*2))
    )) {
        src_w /= 2;
        src_h /= 2;
        reduce++;
    }
    loadPng(reduce);
    return true;
}

// load from PNG File
bool ImageBox::loadPngFile(uint16_t file_idx, uint64_t pos, size_t size, bool is_unsync)
{
    int decoded;
    uint8_t reduce = 0;
    PngDec.abort();
    file_menu_get_obj(file_idx, &file);
    decoded = PngDec.loadSdFile(&file, pos, size, is_unsync);
    if (decoded <= 0) { return false; }
    src_w = PngDec.width;
    src_h = PngDec.height;
    while (resizeFit && (
        (keepAspectRatio && (src_w > width*2 && src_h > height*2)) ||
        (!keepAspectRatio && (src_w > width*2 || src_h > height*2))
    )) {
        src_w /= 2;
        src_h /= 2;
        reduce++;
    }
    loadPng(reduce);
    return true;
}

void ImageBox::loadPng(uint8_t reduce)
{
    if (image == NULL) { return; }
    src_w = PngDec.width;
    src_h = PngDec.height;
    //PngDec.linkImageBox(this);
    PngDec.set_draw_callback(this, cb_pngdec_draw_with_resize);
    #ifdef DEBUG_LCD_ELEMENT_BOX
    { // DEBUG
        char str[256];
        sprintf(str, "PNG info: (w, h) = (%d, %d)", src_w, src_h);
        Serial.println(str);
    }
    #endif // DEBUG_LCD_ELEMENT_BOX
    if (reduce) {
        src_w /= 1<<reduce;
        src_h /= 1<<reduce;
        #ifdef DEBUG_LCD_ELEMENT_BOX
        { // DEBUG
            char str[256];
            sprintf(str, "Reduce applied:  (w, h) = (%d, %d)", src_w, src_h);
            Serial.println(str);
        }
        #endif // DEBUG_LCD_ELEMENT_BOX
    }
    if (resizeFit) {
        ratio256_w = width * 256 / src_w;
        ratio256_h = height * 256 / src_h;
        if (keepAspectRatio) {
            uint32_t ratio256 = (ratio256_w < ratio256_h) ? ratio256_w : ratio256_h;
            ratio256_w = ratio256;
            ratio256_h = ratio256;
        }
    } else {
        ratio256_w = 256;
        ratio256_h = 256;
    }
    if (PngDec.decode(reduce)) {
        isLoaded = true;
    }
    if (img_w < width) { // delete horizontal blank
        for (int16_t plot_y = 1; plot_y < img_h; plot_y++) {
            memmove(&image[img_w*plot_y], &image[width*plot_y], img_w*2);
        }
    }
    #ifdef DEBUG_LCD_ELEMENT_BOX
    { // DEBUG
        char str[256];
        sprintf(str, "Resized to (img_w, img_h) = (%d, %d)", img_w, img_h);
        Serial.println(str);
    }
    #endif // DEBUG_LCD_ELEMENT_BOX
}

void ImageBox::unload()
{
    img_w = img_h = 0;
    src_w = src_h = 0;
    ratio256_w = ratio256_h = 256;
    memset(image, 0, width * height * 2);
    isLoaded = false;
}

void ImageBox::deleteAll()
{
    isUpdated = true;
    decode_ok = true;
    isLoaded = false;
    changeNext = true;
    image_count = 0;
    image_idx = 0;
    unload();
}

int ImageBox::getCount()
{
    return image_count;
}

//=================================
// Implementation of IconBox class
//=================================
IconBox::IconBox(int16_t pos_x, int16_t pos_y, uint16_t fgColor, uint16_t bgColor)
    : isUpdated(true), pos_x(pos_x), pos_y(pos_y), fgColor(fgColor), bgColor(bgColor), icon(NULL) {}

IconBox::IconBox(int16_t pos_x, int16_t pos_y, const uint8_t *icon, uint16_t fgColor, uint16_t bgColor)
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

void IconBox::draw(Adafruit_SPITFT *tft)
{
    if (!isUpdated) { return; }
    isUpdated = false;
    clear(tft);
    if (icon != NULL) {
        tft->drawBitmap(pos_x, pos_y, icon, iconWidth, iconHeight, fgColor);
    }
}

void IconBox::clear(Adafruit_SPITFT *tft)
{
    tft->fillRect(pos_x, pos_y, iconWidth, iconHeight, bgColor); // clear Icon rectangle
}

void IconBox::setIcon(const uint8_t *icon)
{
    if (this->icon == icon) { return; }
    this->icon = icon;
    update();
}

//=================================
// Implementation of TextBox class
//=================================
TextBox::TextBox(int16_t pos_x, int16_t pos_y, uint16_t fgColor, uint16_t bgColor)
    : isUpdated(true), pos_x(pos_x), pos_y(pos_y), fgColor(fgColor), bgColor(bgColor), align(LcdElementBox::AlignLeft), str(""), encoding(none) {}

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

void TextBox::draw(Adafruit_SPITFT *tft)
{
    if (!isUpdated) { return; }
    int16_t x_ofs;
    isUpdated = false;
    TextBox::clear(tft); // call clear() of this class
    //tft->fillRect(x0, y0, w0, h0, bgColor); // clear previous rectangle
    tft->setTextColor(fgColor);
    x_ofs = (align == LcdElementBox::AlignRight) ? -w0 : (align == LcdElementBox::AlignCenter) ? -w0/2 : 0; // at this point, w0 could be not correct
    tft->getTextBounds(str, pos_x+x_ofs, pos_y, &x0, &y0, &w0, &h0, encoding); // update next smallest rectangle (get correct w0)
    x_ofs = (align == LcdElementBox::AlignRight) ? -w0 : (align == LcdElementBox::AlignCenter) ? -w0/2 : 0;
    tft->getTextBounds(str, pos_x+x_ofs, pos_y, &x0, &y0, &w0, &h0, encoding); // update next smallest rectangle (get total correct info)
    tft->setCursor(pos_x+x_ofs, pos_y);
    tft->println(str, encoding);
}

void TextBox::clear(Adafruit_SPITFT *tft)
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
NFTextBox::NFTextBox(int16_t pos_x, int16_t pos_y, uint16_t width, uint16_t fgColor, uint16_t bgColor, const GFXfont *f, int16_t ofs_y, uint16_t height_y)
    : TextBox(pos_x, pos_y, "", LcdElementBox::AlignLeft, fgColor, bgColor), Font(f), ofs_y(ofs_y), height_y(height_y), width(width), draw_count(0), blink(false)
{
    initCanvas();
}

NFTextBox::NFTextBox(int16_t pos_x, int16_t pos_y, uint16_t width, align_enm align, uint16_t fgColor, uint16_t bgColor, const GFXfont *f, int16_t ofs_y, uint16_t height_y)
    : TextBox(pos_x, pos_y, "", align, fgColor, bgColor), Font(f), ofs_y(ofs_y), height_y(height_y), width(width), draw_count(0), blink(false)
{
    initCanvas();
}

NFTextBox::NFTextBox(int16_t pos_x, int16_t pos_y, uint16_t width, const char *str, align_enm align, uint16_t fgColor, uint16_t bgColor, const GFXfont *f, int16_t ofs_y, uint16_t height_y)
    : TextBox(pos_x, pos_y, str, align, fgColor, bgColor), Font(f), ofs_y(ofs_y), height_y(height_y), width(width), draw_count(0), blink(false)
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
    canvas = new GFXcanvas1(width, height_y); // 2x width for minimize canvas update, which costs Unifont SdFat access
    canvas->setFont(Font, ofs_y);
    canvas->setTextWrap(false);
    canvas->setTextSize(1);
    canvas->getTextBounds(str, 0, 0, &x0, &y0, &w0, &h0, encoding); // idle-run because first time fails somehow
}

void NFTextBox::draw(Adafruit_SPITFT *tft)
{
    if (!isUpdated && !(blink && draw_count % (BlinkInterval/2) == 0)) { draw_count++; return; }
    int16_t x_ofs;
    int16_t new_x_ofs;
    uint16_t new_w0;
    isUpdated = false;
    x_ofs = (align == LcdElementBox::AlignRight) ? -w0 : (align == LcdElementBox::AlignCenter) ? -w0/2 : 0; // previous x_ofs
    canvas->getTextBounds(str, 0, 0, &x0, &y0, &new_w0, &h0, encoding); // get next smallest rectangle (get correct new_w0)
    new_x_ofs = (align == LcdElementBox::AlignRight) ? -new_w0 : (align == LcdElementBox::AlignCenter) ? -new_w0/2 : 0;
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
    //tft->drawBitmap(pos_x+x_ofs, pos_y, canvas->getBuffer(), width, height_y, fgColor, bgColor);
    if (blink && (draw_count % BlinkInterval >= (BlinkInterval/2))) { // Blink (Disappear) case
        tft->fillRect(pos_x+x_ofs, pos_y, width, height_y, bgColor);
    } else { // Normal case + Blink (Appear) case
        int16_t x = pos_x+x_ofs;
        int16_t y = pos_y;
        const uint8_t *bitmap = canvas->getBuffer();
        int16_t w = width;
        int16_t h = height_y;
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
    draw_count++;
}

void NFTextBox::clear(Adafruit_SPITFT *tft)
{
    int16_t x_ofs = (align == LcdElementBox::AlignRight) ? -w0 : (align == LcdElementBox::AlignCenter) ? -w0/2 : 0; // previous x_ofs
    tft->fillRect(pos_x+x_ofs, pos_y+y0, w0, h0, bgColor);
}

void NFTextBox::setText(const char *str, encoding_t encoding)
{
    if (strncmp(this->str, str, charSize) == 0) { return; }
    this->encoding = encoding;
    update();
    //strncpy(this->str, str, charSize);
    memcpy(this->str, str, charSize);
    canvas->fillRect(0, 0, width, height_y, bgColor);
    canvas->setCursor(0, 0);
    canvas->print(str, encoding);
}

void NFTextBox::setBlink(bool blink)
{
    if (this->blink == blink) { return; }
    this->blink = blink;
    update();
}

//=================================
// Implementation of IconTextBox class
//=================================
IconTextBox::IconTextBox(int16_t pos_x, int16_t pos_y, const uint8_t *icon, uint16_t fgColor, uint16_t bgColor)
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

void IconTextBox::draw(Adafruit_SPITFT *tft)
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

void IconTextBox::clear(Adafruit_SPITFT *tft)
{
    iconBox.clear(tft);
    TextBox::clear(tft);
}

void IconTextBox::setIcon(const uint8_t *icon)
{
    iconBox.setIcon(icon);
}

//=================================
// Implementation of ScrollTextBox class < LcdElementBox
//=================================
ScrollTextBox::ScrollTextBox(int16_t pos_x, int16_t pos_y, uint16_t width, uint16_t fgColor, uint16_t bgColor, const GFXfont *f, int16_t ofs_y, uint16_t height_y)
    : isUpdated(true), pos_x(pos_x), pos_y(pos_y), fgColor(fgColor), bgColor(bgColor), Font(f), ofs_y(ofs_y), height_y(height_y),
      str(""), width(width), stay_count(0), scr_en(true), encoding(none)
{
    int16_t x0, y0; // dummy
    uint16_t h0; // dummy
    canvas = new GFXcanvas1(width*2, height_y); // 2x width for minimize canvas update, which costs Unifont SdFat access
    canvas->setFont(Font, ofs_y);
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
void ScrollTextBox::draw(Adafruit_SPITFT *tft)
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
                        canvas->fillRect(0, 0, width*2, height_y, bgColor);
                        canvas->setCursor(0, 0);
                        canvas->print(str, encoding);
                    }
                    x_ofs = 0;
                }
                stay_count = 0;
            }
        } else {
            if ((x_ofs % width) == -(width-1)) { // modulo returns minus value (C++)
                canvas->fillRect(0, 0, width*2, height_y, bgColor);
                canvas->setCursor((x_ofs-1)/width*width, 0);
                canvas->print(str, encoding);
            }
            x_ofs--;
        }
    }
    if (!isUpdated && stay_count != 0) { return; }
    isUpdated = false;

    // Flicker less draw (width must be ScrollTextBox's width)
    //tft->drawBitmap(pos_x+x_ofs, pos_y, canvas->getBuffer(), width, height_y, fgColor, bgColor);
    {
        int16_t x = pos_x + x_ofs%width;
        int16_t y = pos_y;
        const uint8_t *bitmap = canvas->getBuffer();
        int16_t w = width * 2;
        int16_t h = height_y;
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

void ScrollTextBox::clear(Adafruit_SPITFT *tft)
{
    tft->fillRect(pos_x, pos_y, width, height_y, bgColor);
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

    canvas->fillRect(0, 0, width*2, height_y, bgColor);
    canvas->setCursor(0, 0);
    canvas->print(str, encoding);
}

//=================================
// Implementation of IconScrollTextBox class < ScrollTextBox
//=================================
IconScrollTextBox::IconScrollTextBox(int16_t pos_x, int16_t pos_y, uint16_t width, uint16_t fgColor, uint16_t bgColor, const GFXfont *f, int16_t ofs_y, uint16_t height_y)
    : ScrollTextBox(pos_x+16, pos_y, width-16, fgColor, bgColor, f, ofs_y, height_y), iconBox(pos_x, pos_y, fgColor, bgColor) {}

IconScrollTextBox::IconScrollTextBox(int16_t pos_x, int16_t pos_y, const uint8_t *icon, uint16_t width, uint16_t fgColor, uint16_t bgColor, const GFXfont *f, int16_t ofs_y, uint16_t height_y)
    : ScrollTextBox(pos_x+16, pos_y, width-16, fgColor, bgColor, f, ofs_y, height_y), iconBox(pos_x, pos_y, icon, fgColor, bgColor) {}

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

void IconScrollTextBox::draw(Adafruit_SPITFT *tft)
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

void IconScrollTextBox::clear(Adafruit_SPITFT *tft)
{
    iconBox.clear(tft);
    ScrollTextBox::clear(tft);
}

void IconScrollTextBox::setIcon(const uint8_t *icon)
{
    iconBox.setIcon(icon);
}