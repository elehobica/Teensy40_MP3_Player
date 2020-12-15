/*-----------------------------------------------------------/
/ PNGDecoder for pngle
/------------------------------------------------------------/
/ Copyright (c) 2020, Elehobica
/ Released under the BSD-2-Clause
/ refer to https://opensource.org/licenses/BSD-2-Clause
/-----------------------------------------------------------*/

#include "PNGDecoder.h"

PNGDecoder PngDec;

PNGDecoder::PNGDecoder()
{
	decode_state = none;
	thisPtr = this;
	pngle = NULL;
	cb_obj = NULL;
	pngDecoder_draw_rgb565_callback = NULL;
	file = NULL;
}

PNGDecoder::~PNGDecoder()
{
	if (pngle) {
		pngle_destroy(pngle);
	}
	cb_obj = NULL;
	pngDecoder_draw_rgb565_callback = NULL;
}

void PNGDecoder::set_draw_callback(void *cb_obj, pngDecoder_draw_rgb565_callback_t pngDecoder_draw_rgb565_callback)
{
	this->cb_obj = cb_obj;
	this->pngDecoder_draw_rgb565_callback = pngDecoder_draw_rgb565_callback;
}

void PNGDecoder::abort()
{
	decode_state = none;
	if (pngle) {
		pngle_destroy(pngle);
	}
	if (png_src_type == png_sd_file) {
		if (file) file->close();
	}
	cb_obj = NULL;
	pngDecoder_draw_rgb565_callback = NULL;
}

int PNGDecoder::loadSdFile(MutexFsBaseFile *pngFile, uint64_t file_pos, size_t file_size)
{
	png_src_type = png_sd_file;
	file = pngFile;

	if (!file) {
		Serial.println("ERROR: PNGDecoder SdFat is not ready!");
		return -1;
	}

	png_ofs = 0; // PNG binary offset (not file position)
	if (file_pos == 0) {
		size = file->size();
	} else {
		if (!file->seekSet(file_pos)) {
			Serial.println("ERROR: PNGDecoder seekSet failed");
			return -1;
		}
		size = file_size;
	}
	return preDecode();
}

int PNGDecoder::loadArray(const uint8_t array[], uint32_t  array_size)
{
	png_src_type = png_array;
	png_ofs = 0;
	png_ptr = (uint8_t *) array;
	size = array_size;
	return preDecode();
}

// on init callback
void pngle_callback(pngle_t *pngle, uint32_t w, uint32_t h)
{
	PNGDecoder *thisPtr = PngDec.thisPtr;
	thisPtr->decode_state = PNGDecoder::init_done;
	thisPtr->width = w;
	thisPtr->height = h;
}

// on draw callback
void pngle_callback(pngle_t *pngle, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint8_t rgba[4])
{
	PNGDecoder *thisPtr = PngDec.thisPtr;
	if (thisPtr->cb_obj == NULL || thisPtr->pngDecoder_draw_rgb565_callback == NULL) { return; }
	thisPtr->decode_state = PNGDecoder::decoding;
	uint16_t rgb565 = (((uint16_t) rgba[0] & 0xf8) << 8) | (((uint16_t) rgba[1] & 0xfc) << 3) | ((uint16_t) rgba[2] >> 3);
	thisPtr->pngDecoder_draw_rgb565_callback(thisPtr->cb_obj, x, y, rgb565);
}

// on done callback
void pngle_callback(pngle_t *pngle)
{
	PNGDecoder *thisPtr = PngDec.thisPtr;
	thisPtr->decode_state = PNGDecoder::decode_done;
}

int PNGDecoder::preDecode()
{
	decode_state = none;
	if (png_src_type != png_sd_file && png_src_type != png_array) { return 0; }

	remain = size;
	unfed = 0;

	pngle = pngle_new();
    pngle_set_init_callback(pngle, pngle_callback);
    pngle_set_draw_callback(pngle, pngle_callback);
    pngle_set_done_callback(pngle, pngle_callback);

	while (1) {
		int len;
		if (png_src_type == png_sd_file) {
			len = file->read(buf + unfed, sizeof(buf) - unfed);
		} else { // png_src_type == png_array
			len = (remain <= (int) sizeof(buf)) ? sizeof(buf) : remain;
			memmove(buf, png_ptr + size - remain, len);
		}
		remain -= len;
		if (len <= 0) { break; }
		int fed = pngle_feed(pngle, buf, unfed + len);
		if (fed < 0) {
			char str[256];
			sprintf(str, "ERROR: PNGDecoder pngle %s", pngle_error(pngle));
			Serial.println(str);
			return -1;
		}
		unfed += len - fed;
		if (unfed > 0) { memmove(buf, buf + fed, unfed); }
		if (remain <= 0 || decode_state == init_done) {
			break;
		}
	}
	return 1;
}

int PNGDecoder::decode(uint8_t reduce)
{
	if (png_src_type != png_sd_file && png_src_type != png_array) { return 0; }
	if (decode_state != init_done && decode_state != decoding) {
		abort();
		return 0;
	}
	pngle_set_reduce(pngle, reduce);
	while (1) {
		int len;
		if (png_src_type == png_sd_file) {
			len = file->read(buf + unfed, sizeof(buf) - unfed);
		} else { // png_src_type == png_array
			len = (remain <= (int) sizeof(buf)) ? sizeof(buf) : remain;
			memmove(buf, png_ptr + size - remain, len);
		}
		remain -= len;
		if (len <= 0) { break; }
		int fed = pngle_feed(pngle, buf, unfed + len);
		if (fed < 0) {
			char str[256];
			sprintf(str, "ERROR: PNGDecoder pngle %s", pngle_error(pngle));
			Serial.println(str);
			return 0;
		}
		unfed += len - fed;
		if (unfed > 0) { memmove(buf, buf + fed, unfed); }
		if (remain <= 0 || decode_state == decode_done) {
			break;
		}
	}
	return 1;
}