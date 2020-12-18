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

#define WAV_SD_BUF_SIZE	(1152*4)
#define WAV_BUF_SIZE	(WAV_SD_BUF_SIZE/2)
#define DECODE_NUM_STATES 2									//How many steps in decode() ?

#define MIN_FREE_RAM (35+1)*1024 // 1KB Reserve

//#define CODEC_DEBUG

static AudioPlaySdWav 	*wavobjptr;

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
				samprate =      (unsigned short) (*((uint32_t *) (sd_buf + ofs + 4 + 4 + 2 + 2))); // samplerate
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

	buf[0] = (short *) malloc(WAV_BUF_SIZE * sizeof(int16_t));
	buf[1] = (short *) malloc(WAV_BUF_SIZE * sizeof(int16_t));

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
	if (!skip || samprate != AUDIOCODECS_SAMPLE_RATE) {
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

	_VectorsRam[IRQ_AUDIOCODEC + 16] = &decodeWav;
	initSwi();

	decoded_length[0] = 0;
	decoded_length[1] = 0;
	decoding_block = 0;
	decoding_state = 0;

	play_pos = 0;

	sd_p = sd_buf;

	for (size_t i=0; i< DECODE_NUM_STATES; i++) {
		decodeWav_core();
	}

	// For Resume play with 'position'
	if (position != 0 && position < fsize()) {
		fseek(position);
		this->samples_played += samples_played;
		// Replace sd_buf data after sd_p with Frame Data in 'position'

		// [Method 1]: just replace data after sd_p with Frame Data in 'position'
		//fread(sd_p, sd_left);

		// [Method 2]: set sd_p = sd_buf and fill sd_buf fully with Frame Data in 'position'
		sd_left = fillReadBuffer(sd_buf, sd_p, 0, WAV_SD_BUF_SIZE);
		sd_p = sd_buf;
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

		memcpy_frominterleaved(&block_left->data[0], &block_right->data[0], buf[playing_block] + pl);

		pl += AUDIO_BLOCK_SAMPLES * 2;
		transmit(block_left, 0);
		transmit(block_right, 1);
		release(block_right);
		decoded_length[playing_block] -= AUDIO_BLOCK_SAMPLES * 2;

	} else
	{
		// if we're playing mono, no right-side block
		// let's do a (hopefully good optimized) simple memcpy
		memcpy(block_left->data, buf[playing_block] + pl, AUDIO_BLOCK_SAMPLES * sizeof(short));

		pl += AUDIO_BLOCK_SAMPLES;
		transmit(block_left, 0);
		transmit(block_left, 1);
		decoded_length[playing_block] -= AUDIO_BLOCK_SAMPLES;

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
	//Serial.println("decodeWave_core");

	/*
	Serial.print("decodeWav called: ");
	Serial.println(millis());
	*/
	int eof = false;
	uint32_t cycles = ARM_DWT_CYCCNT;

	if ( o->decoded_length[db] > 0 ) {
		return; //this block is playing, do NOT fill it
	}

	switch (o->decoding_state) {

	case 0:
		{
			o->sd_left = o->fillReadBuffer(o->sd_buf, o->sd_p, o->sd_left, WAV_SD_BUF_SIZE);
			if (!o->sd_left) { eof = true; goto wavend; }
			o->sd_p = o->sd_buf;

			uint32_t cycles_rd = (ARM_DWT_CYCCNT - cycles);
			if (cycles_rd > o->decode_cycles_max_read )o-> decode_cycles_max_read = cycles_rd;
			break;
		}

	case 1:
		{
			int decode_res = 0;
			int i;
			//Serial.println(o->sd_left);
			for (i = 0; i < o->sd_left; i+=2) {
				o->buf[db][i/2] = *((short *) &o->sd_p[i]);
				if (i/2 >= WAV_BUF_SIZE) { break; }
			}
			o->sd_p += i;
			o->sd_left -= i;
			//Serial.println(i/2);

			switch(decode_res)
			{
				case 0:
				{
					o->err_cnt = 0;
					o->decoded_length[db] = i/2;
					break;
				}

				default :
				{
					if (o->err_cnt++ < 1) { // Ignore Error
						o->decoded_length[db] = i/2;
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
			break;
		}
	}//switch

wavend:

	o->decoding_state++;
	if (o->decoding_state >= DECODE_NUM_STATES) o->decoding_state = 0;

	if (eof) {
		//o->stop_for_next();
		o->stop();
	}

	/*
	Serial.print("decodeWav wavend: ");
	Serial.println(millis());
	*/
}

#if 0
void AudioPlaySdWav::stop_for_next(void)
{
	//NVIC_DISABLE_IRQ(IRQ_AUDIOCODEC);
	playing = codec_stopped;
	/*
	if (buf[1]) {free(buf[1]);buf[1] = NULL;}
	if (buf[0]) {free(buf[0]);buf[0] = NULL;}
	freeBuffer();
	*/
	fclose();
	wavobjptr = NULL;
}
#endif

// lengthMillis (Override)
unsigned AudioPlaySdWav::lengthMillis(void)
{
	return max((uint64_t) data_size * 1000 / ((uint32_t) samprate * _channels * bitsPerSample/8), positionMillis());
}
