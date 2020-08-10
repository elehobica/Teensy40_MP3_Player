#ifndef _PNG_DECODER_H_
#define _PNG_DECODER_H_

#include "pngle/pngle.h"
#include <SdFat.h>

typedef void (*pngDecoder_draw_rgb565_callback_t) (void *obj, uint32_t x, uint32_t y, uint16_t rgb565);

class PNGDecoder {
public:
	typedef enum _png_src_type_t {
		png_array = 0,
		png_sd_file
	} png_src_type_t;
	typedef enum _png_decode_state {
		none = 0,
		init_done,
		decoding,
		decode_done
	} png_decode_state_t;
	PNGDecoder *thisPtr;
	uint16_t width, height;
	PNGDecoder();
	~PNGDecoder();
	void set_draw_callback(void *cb_obj, pngDecoder_draw_rgb565_callback_t pngDecoder_draw_rgb565_callback);
	void abort();
	int loadSdFile(FsBaseFile pngFile, uint64_t file_pos = 0, size_t file_size = 0);
	int loadArray(const uint8_t array[], uint32_t  array_size);
	int decode(uint8_t reduce = 0);
private:
	png_src_type_t png_src_type;
	pngle_t *pngle;
	//ImageBox *imageBox;
	void *cb_obj;
	pngDecoder_draw_rgb565_callback_t pngDecoder_draw_rgb565_callback;
	FsBaseFile file;
	uint64_t png_ofs;
	size_t size;
	uint8_t *png_ptr;
	char buf[256];
	int remain;
	int unfed;
	volatile png_decode_state_t decode_state;
	int preDecode();
	//void pngle_on_init_callback(pngle_t *pngle, uint32_t w, uint32_t h);
	//void pngle_on_draw_callback(pngle_t *pngle, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint8_t rgba[4]);
	friend void pngle_callback(pngle_t *pngle, uint32_t w, uint32_t h);
	friend void pngle_callback(pngle_t *pngle, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint8_t rgba[4]);
	friend void pngle_callback(pngle_t *pngle);
};

extern PNGDecoder PngDec;

#endif //_PNG_DECODER_H_