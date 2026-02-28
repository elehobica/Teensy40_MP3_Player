/*
	Arduino Audiocodecs

	Copyright (c) 2014 Frank Bosing

	This library is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This library is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this library.  If not, see <http://www.gnu.org/licenses/>.

	The helix decoder itself as a different license, look at the subdirectories for more info.

	Diese Bibliothek ist freie Software: Sie konnen es unter den Bedingungen
	der GNU General Public License, wie von der Free Software Foundation,
	Version 3 der Lizenz oder (nach Ihrer Wahl) jeder neueren
	veroffentlichten Version, weiterverbreiten und/oder modifizieren.

	Diese Bibliothek wird in der Hoffnung, dass es nutzlich sein wird, aber
	OHNE JEDE GEWAHRLEISTUNG, bereitgestellt; sogar ohne die implizite
	Gewahrleistung der MARKTFAHIGKEIT oder EIGNUNG FUR EINEN BESTIMMTEN ZWECK.
	Siehe die GNU General Public License fur weitere Details.

	Sie sollten eine Kopie der GNU General Public License zusammen mit diesem
	Programm erhalten haben. Wenn nicht, siehe <http://www.gnu.org/licenses/>.

	Der Helixdecoder selbst hat eine eigene Lizenz, bitte fur mehr Informationen
	in den Unterverzeichnissen nachsehen.

 */

 /* The Helix-Library is modified for Teensy 3.1 */

// Total RAM Usage: ??? KB

// Written by Elehobica for Teensy 4.0 MP3 Player based on play_sd_mp3 / play_sd_aac scheme

#include "play_sd_wav.h"
#include <TeensyThreads.h>

extern Threads::Event codec_event;

#define WAV_SD_BUF_SIZE	(1152*16)
#define WAV_BUF_SIZE	(WAV_SD_BUF_SIZE/2)
#define DECODE_NUM_STATES 1									//SD read + decode in single step

#define MIN_FREE_RAM (35+1)*1024 // 1KB Reserve

//#define CODEC_DEBUG

static AudioPlaySdWav 	*wavobjptr;

static bool isValidSampleRate(unsigned int sr) {
	return (sr == 44100 || sr == 48000 || sr == 88200 ||
	        sr == 96000 || sr == 176400 || sr == 192000);
}

void decodeWav(void);

size_t AudioPlaySdWav::parseFmtChunk(uint8_t *sd_buf, size_t sd_buf_size)
{
	size_t ofs = 0;
	if (sd_buf[ 0]=='R' && sd_buf[ 1]=='I' && sd_buf[ 2]=='F' && sd_buf[ 3]=='F' &&
	    sd_buf[ 8]=='W' && sd_buf[ 9]=='A' && sd_buf[10]=='V' && sd_buf[11]=='E')
	{
		ofs = 12;
		while (1) {
			char *chunk_id = (char *) (sd_buf + ofs);
			uint32_t *size = (uint32_t *) (sd_buf + ofs + 4);
			if (memcmp(chunk_id, "fmt ", 4) == 0) {
				_channels =     (unsigned short) (*((uint16_t *) (sd_buf + ofs + 4 + 4 + 2))); // channels
				samprate =      *((uint32_t *) (sd_buf + ofs + 4 + 4 + 2 + 2)); // samplerate
				bitrate =       (unsigned short) (*((uint32_t *) (sd_buf + ofs + 4 + 4 + 2 + 2 + 4)) /* bytepersec */ * 8 / 1000); // Kbps
				bitsPerSample = (unsigned short) (*((uint16_t *) (sd_buf + ofs + 4 + 4 + 2 + 2 + 4 + 4 + 2))); // bitswidth
				break;
			}
			ofs += 8 + *size;
			if (ofs > sd_buf_size - 8) { return 0; };
		}
	}
	return ofs;
}

void AudioPlaySdWav::stop(void)
{
	NVIC_DISABLE_IRQ(IRQ_AUDIOCODEC);
	playing = codec_stopped;
	if (buf[1]) {free(buf[1]);buf[1] = NULL;}
	if (buf[0]) {free(buf[0]);buf[0] = NULL;}
	freeBuffer();
	fclose();
	wavobjptr = NULL;
}
/*
float AudioPlaySdWav::processorUsageMaxDecoder(void){
	//this is somewhat incorrect, it does not take the interruptions of update() into account -
	//therefore the returned number is too high.
	//Todo: better solution
	return (decode_cycles_max / (0.026*F_CPU)) * 100;
};

float AudioPlaySdWav::processorUsageMaxSD(void){
	//this is somewhat incorrect, it does not take the interruptions of update() into account -
	//therefore the returned number is too high.
	//Todo: better solution
	return (decode_cycles_max_sd / (0.026*F_CPU)) * 100;
};
*/

#if 0
int AudioPlaySdWav::standby_play(MutexFsBaseFile *file)
{

	while (isPlaying()) { /*delay(1);*/ } // Wait for previous WAV playing to stop

	//fclose();
	//NVIC_DISABLE_IRQ(IRQ_AUDIOCODEC);

	// instance copy start
	initVars();
	wavobjptr = this;
	ftype=codec_file;
	fptr=NULL;
	_file = MutexFsBaseFile(*file);
	_fsize=_file.fileSize();

	// instance copy done

	lastError = ERR_CODEC_NONE;

	//Fill buffer from the beginning with fresh data
	sd_left = fillReadBuffer(sd_buf, sd_buf, 0, WAV_SD_BUF_SIZE);
	// parse 'fmt ' chunk
	int skip = parseFmtChunk(sd_buf, WAV_SD_BUF_SIZE);
	if (!skip || bitsPerSample != 16 || samprate != AUDIOCODECS_SAMPLE_RATE) {
		Serial.println("incompatible WAV file.");
		lastError = ERR_CODEC_FORMAT;
		stop();
		return lastError;
	}
	// skip to 'data' chunk
	skip = skipToDataChunk(sd_buf, WAV_SD_BUF_SIZE);
	data_size = *((uint32_t *) (sd_buf + skip + 4));
	sd_p = sd_buf + skip + 8;
	sd_left -= skip + 8;

	/* // DEBUG
	Serial.print("Data Chunk skip: ");
	Serial.println(skip);
	*/

	//Fill buffer from the beginning with fresh data
	sd_left = fillReadBuffer(sd_buf, sd_p, sd_left, WAV_SD_BUF_SIZE);

	if (!sd_left) {
		lastError = ERR_CODEC_FILE_NOT_FOUND;
		stop();
		return lastError;
	}

	//_VectorsRam[IRQ_AUDIOCODEC + 16] = &decodeWav;
	//initSwi();

	decoded_length[0] = 0;
	decoded_length[1] = 0;
	decoding_block = 0;
	decoding_state = 0;

	play_pos = 0;

	sd_p = sd_buf;

	for (size_t i=0; i< DECODE_NUM_STATES; i++) {
		decodeWav_core();
	}

	decoding_block = 1;

	playing = codec_playing;
	/*
	Serial.print("playing = codec_playing: ");
	Serial.println(millis());
	*/

#ifdef CODEC_DEBUG
//	Serial.printf("RAM: %d\r\n",ram-freeRam());

#endif
    return lastError;
}
#endif

int AudioPlaySdWav::play(size_t position, unsigned samples_played)
{
	/*
	Serial.print("play: ");
	Serial.println(millis());
	*/
	lastError = ERR_CODEC_NONE;
	initVars();

	sd_buf = allocBuffer(WAV_SD_BUF_SIZE);
	if (!sd_buf) return ERR_CODEC_OUT_OF_MEMORY;

	wavobjptr = this;

	buf[0] = (int32_t *) malloc(WAV_BUF_SIZE * sizeof(int32_t));
	buf[1] = (int32_t *) malloc(WAV_BUF_SIZE * sizeof(int32_t));

	if (!buf[0] || !buf[1])
	{
		lastError = ERR_CODEC_OUT_OF_MEMORY;
		stop();
		return lastError;
	}

	/*
	Serial.print("obj ready: ");
	Serial.println(millis());
	*/

	//Fill buffer from the beginning with fresh data
	sd_left = fillReadBuffer(sd_buf, sd_buf, 0, WAV_SD_BUF_SIZE);
	// parse 'fmt ' chunk
	int skip = parseFmtChunk(sd_buf, WAV_SD_BUF_SIZE);
	if (!skip || !isValidSampleRate(samprate) || (bitsPerSample != 16 && bitsPerSample != 24)) {
		Serial.println("incompatible WAV file.");
		lastError = ERR_CODEC_FORMAT;
		stop();
		return lastError;
	}
	// skip to 'data' chunk
	skip = skipToDataChunk(sd_buf, WAV_SD_BUF_SIZE);
	data_size = *((uint32_t *) (sd_buf + skip + 4));
	data_remaining = data_size;
	sd_p = sd_buf + skip + 8;
	sd_left -= skip + 8;

	//Fill buffer from the beginning with fresh data
	sd_left = fillReadBuffer(sd_buf, sd_p, sd_left, WAV_SD_BUF_SIZE);

	if (!sd_left) {
		lastError = ERR_CODEC_FILE_NOT_FOUND;
		stop();
		return lastError;
	}

	_VectorsRam[IRQ_AUDIOCODEC + 16] = &decodeWav;
	initSwi();

	decoded_length[0] = 0;
	decoded_length[1] = 0;
	decoding_block = 0;
	decoding_state = 0;

	play_pos = 0;

	sd_p = sd_buf;

	// For Resume play with 'position'
	if (position != 0 && position < fsize()) {
		fseek(position);
		this->samples_played += samples_played;
		sd_left = 0;  // force fresh read from new position
		sd_p = sd_buf;
	}

	// Pre-fill both buffers before starting playback
	decodeWav_core();          // fills buffer 0
	decoding_block = 1;
	decodeWav_core();          // fills buffer 1

	// decoding_block stays at 1, so playing_block = 0 (play buffer 0 first)
	playing = codec_playing;

    return lastError;
}

//runs in ISR
__attribute__ ((optimize("O2")))
void AudioPlaySdWav::update(void)
{
	audio_block_t	*block_left;
	audio_block_t	*block_right;

	//paused or stopped ?
	if (playing != codec_playing) return;

	//chain decoder-interrupt.
	//to give the user-sketch some cpu-time, only chain
	//if the swi is not active currently.
	//In addition, check before if there waits work for it.
	int db = decoding_block;
	if (!NVIC_IS_ACTIVE(IRQ_AUDIOCODEC))
		if (decoded_length[db]==0)
			NVIC_TRIGGER_INTERRUPT(IRQ_AUDIOCODEC);

	//determine the block we're playing from
	int playing_block = 1 - db;
	if (decoded_length[playing_block] <= 0) {
		if (decoded_length[db] > 0) {
			Serial.println("[play_sd_wav] Swap block for rescue");
			decoding_block = playing_block;
			playing_block = db;
		} else {
			return;
		}
	}

	// allocate the audio blocks to transmit
	block_left = allocate();
	if (block_left == NULL) return;

	uintptr_t pl = play_pos;

	if (_channels == 2) {
		// if we're playing stereo, allocate another
		// block for the right channel output
		block_right = allocate();
		if (block_right == NULL) {
			release(block_left);
			return;
		}

		{
			int32_t *src = buf[playing_block] + pl;
			int valid_pairs = min(AUDIO_BLOCK_SAMPLES, decoded_length[playing_block] / 2);
			int i;
			for (i = 0; i < valid_pairs; i++) {
				block_left->data[i]  = src[i * 2];
				block_right->data[i] = src[i * 2 + 1];
			}
			// Zero-fill remaining samples to avoid noise at end of file
			for (; i < AUDIO_BLOCK_SAMPLES; i++) {
				block_left->data[i]  = 0;
				block_right->data[i] = 0;
			}
		}

		pl += AUDIO_BLOCK_SAMPLES * 2;
		transmit(block_left, 0);
		transmit(block_right, 1);
		release(block_right);
		decoded_length[playing_block] -= min((int) decoded_length[playing_block], AUDIO_BLOCK_SAMPLES * 2);

	} else
	{
		// if we're playing mono, no right-side block
		{
			int32_t *src = buf[playing_block] + pl;
			int valid_samples = min(AUDIO_BLOCK_SAMPLES, decoded_length[playing_block]);
			int i;
			for (i = 0; i < valid_samples; i++) {
				block_left->data[i] = src[i];
			}
			// Zero-fill remaining samples to avoid noise at end of file
			for (; i < AUDIO_BLOCK_SAMPLES; i++) {
				block_left->data[i] = 0;
			}
		}

		pl += AUDIO_BLOCK_SAMPLES;
		transmit(block_left, 0);
		transmit(block_left, 1);
		decoded_length[playing_block] -= min((int) decoded_length[playing_block], AUDIO_BLOCK_SAMPLES);

	}

	samples_played += AUDIO_BLOCK_SAMPLES;

	release(block_left);

	//Switch to the next block if we have no data to play anymore:
	if (decoded_length[playing_block] == 0)
	{
		decoding_block = playing_block;
		play_pos = 0;
	} else
	play_pos = pl;

}

void decodeWav(void)
{
	codec_event.trigger();
}

//decoding-interrupt
//__attribute__ ((optimize("O2"))) <- does not work here, bug in g++
void decodeWav_core(void)
{
	if (wavobjptr == NULL) return;
	AudioPlaySdWav *o = wavobjptr;
	int db = o->decoding_block;

	int eof = false;
	uint32_t cycles = ARM_DWT_CYCCNT;

	if ( o->decoded_length[db] > 0 ) {
		return; //this block is playing, do NOT fill it
	}

	// SD read + decode in single step to prevent race condition
	// where ISR changes decoding_block between read and decode
	{
		o->sd_left = o->fillReadBuffer(o->sd_buf, o->sd_p, o->sd_left, WAV_SD_BUF_SIZE);
		if (!o->sd_left) { eof = true; goto wavend; }
		o->sd_p = o->sd_buf;

		uint32_t cycles_rd = (ARM_DWT_CYCCNT - cycles);
		if (cycles_rd > o->decode_cycles_max_read ) o->decode_cycles_max_read = cycles_rd;
	}

	{
		int decode_res = 0;
		int bytesPerSample = o->bitsPerSample / 8;
		// Clamp sd_left to data chunk boundary to avoid decoding non-audio data
		if ((size_t)o->sd_left > o->data_remaining) o->sd_left = o->data_remaining;
		int i = 0;  // byte offset into sd_p
		int j = 0;  // sample index into buf[db]
		for (; i + bytesPerSample <= o->sd_left && j < WAV_BUF_SIZE; i += bytesPerSample, j++) {
			if (bytesPerSample == 3) {
				// 24-bit little-endian: sign-extend to int32_t
				uint32_t raw = (uint32_t)o->sd_p[i] | ((uint32_t)o->sd_p[i+1] << 8) | ((uint32_t)o->sd_p[i+2] << 16);
				o->buf[db][j] = (int32_t)(raw << 8) >> 8;
			} else {
				// 16-bit little-endian: shift left 8 to normalize to 24-bit
				o->buf[db][j] = (int32_t)(*((int16_t *)&o->sd_p[i])) << 8;
			}
		}
		o->sd_p += i;
		o->sd_left -= i;
		o->data_remaining -= i;

		// If no samples were decoded (orphan bytes < bytesPerSample remain), treat as EOF
		if (j == 0) { eof = true; goto wavend; }

		switch(decode_res)
		{
			case 0:
			{
				o->err_cnt = 0;
				o->decoded_length[db] = j;
				break;
			}

			default :
			{
				if (o->err_cnt++ < 1) { // Ignore Error
					o->decoded_length[db] = j;
				} else {
					Serial.print("ERROR: wav decode_res: ");
					Serial.println(decode_res);
					AudioPlaySdWav::lastError = decode_res;
					eof = true;
				}
				break;
			}
		}

		cycles = (ARM_DWT_CYCCNT - cycles);
		if (cycles > o->decode_cycles_max ) o->decode_cycles_max = cycles;
	}

wavend:

	if (eof) {
		o->stop();
	}
}

// parseHeader - pre-parse WAV header to get sample rate before play()
unsigned int AudioPlaySdWav::parseHeader(MutexFsBaseFile *file)
{
	uint8_t header_buf[128];
	uint64_t saved_pos = file->curPosition();
	file->seekSet(0);
	int bytes_read = file->read(header_buf, sizeof(header_buf));
	file->seekSet(saved_pos);
	if (bytes_read < 44) return 0;
	size_t ofs = parseFmtChunk(header_buf, bytes_read);
	return (ofs > 0) ? samprate : 0;
}

// positionMillis (Override) - use runtime samprate instead of compile-time AUDIO_SAMPLE_RATE_EXACT
unsigned AudioPlaySdWav::positionMillis(void)
{
	return (unsigned) ((uint64_t) samples_played * 1000 / samprate);
}

// lengthMillis (Override)
unsigned AudioPlaySdWav::lengthMillis(void)
{
	return max((uint64_t) data_size * 1000 / ((uint32_t) samprate * _channels * bitsPerSample/8), positionMillis());
}
