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

// Total RAM Usage: 35 KB

// Modified by Elehobica for Teensy 4.0 MP3 Player

#include "play_sd_mp3.h"
#include <TeensyThreads.h>

extern Threads::Event codec_event;

#define MP3_SD_BUF_SIZE	2048 								//Enough space for a complete stereo frame
#define MP3_BUF_SIZE	(MAX_NCHAN * MAX_NGRAN * MAX_NSAMP) //MP3 output buffer
#define DECODE_NUM_STATES 2									//How many steps in decode() ?

#define MIN_FREE_RAM (35+1)*1024 // 1KB Reserve

//#define CODEC_DEBUG

unsigned	AudioCodec::decode_cycles;
unsigned	AudioCodec::decode_cycles_max;
unsigned	AudioCodec::decode_cycles_read;
unsigned	AudioCodec::decode_cycles_max_read;
short		AudioCodec::lastError;

static AudioPlaySdMp3 	*mp3objptr;

void decodeMp3(void);

void AudioPlaySdMp3::stop(void)
{
	NVIC_DISABLE_IRQ(IRQ_AUDIOCODEC);
	playing = codec_stopped;
	if (buf[1]) {free(buf[1]);buf[1] = NULL;}
	if (buf[0]) {free(buf[0]);buf[0] = NULL;}
	freeBuffer();
	if (hMP3Decoder) {MP3FreeDecoder(hMP3Decoder);hMP3Decoder=NULL;};
	fclose();
	mp3objptr = NULL;
}
/*
float AudioPlaySdMp3::processorUsageMaxDecoder(void){
	//this is somewhat incorrect, it does not take the interruptions of update() into account -
	//therefore the returned number is too high.
	//Todo: better solution
	return (decode_cycles_max / (0.026*F_CPU)) * 100;
};

float AudioPlaySdMp3::processorUsageMaxSD(void){
	//this is somewhat incorrect, it does not take the interruptions of update() into account -
	//therefore the returned number is too high.
	//Todo: better solution
	return (decode_cycles_max_sd / (0.026*F_CPU)) * 100;
};
*/

int AudioPlaySdMp3::play(size_t position, unsigned samples_played)
{
	/*
	Serial.print("play: ");
	Serial.println(millis());
	*/
	lastError = ERR_CODEC_NONE;
	initVars();

	sd_buf = allocBuffer(MP3_SD_BUF_SIZE);
	if (!sd_buf) return ERR_CODEC_OUT_OF_MEMORY;

	mp3objptr = this;

	buf[0] = (short *) malloc(MP3_BUF_SIZE * sizeof(int16_t));
	buf[1] = (short *) malloc(MP3_BUF_SIZE * sizeof(int16_t));

	hMP3Decoder = MP3InitDecoder();
	MP3GetLastFrameInfo(hMP3Decoder, &mp3FrameInfo);

	if (!buf[0] || !buf[1] || !hMP3Decoder)
	{
		lastError = ERR_CODEC_OUT_OF_MEMORY;
		stop();
		return lastError;
	}

	/*
	Serial.print("obj ready: ");
	Serial.println(millis());
	*/

	//Skip Multiple ID3v2 tags, if existent
	size_id3 = 0;
	while (1) {
		//Read-ahead 10 Bytes to detect ID3
		sd_left = fread(sd_buf, 10);
		int skip = skipID3(sd_buf);
		if (skip == 0) break;
		size_id3 += skip + 10;
		fseek(size_id3);
	}
	/* // DEBUG
	Serial.print("ID3 skip: ");
	Serial.println(size_id3);
	*/

	/*
	//This skips only one ID3v2 tag
	//Skip ID3, if existent
	int skip = skipID3(sd_buf);
	int b = 0;
	if (skip) {
		size_id3 = skip;
		b = skip & 0xfffffe00;
		fseek(b);
		sd_left = 0;
	} else size_id3 = 0;
	// DEBUG
	Serial.print("ID3 skip: ");
	Serial.println(b);
	*/

	//Fill buffer from the beginning with fresh data
	sd_left = fillReadBuffer(sd_buf, sd_buf, sd_left, MP3_SD_BUF_SIZE);

	if (!sd_left) {
		lastError = ERR_CODEC_FILE_NOT_FOUND;
		stop();
		return lastError;
	}

	_VectorsRam[IRQ_AUDIOCODEC + 16] = &decodeMp3;
	initSwi();

	decoded_length[0] = 0;
	decoded_length[1] = 0;
	decoding_block = 0;
	decoding_state = 0;

	play_pos = 0;

	sd_p = sd_buf;

	for (size_t i=0; i< DECODE_NUM_STATES; i++) {
		decodeMp3_core(); // need to decode 1st frame to get Xing Header of VBR
	}

	// For Resume play with 'position'
	if (position != 0 && position < fsize()) {
		fseek(position);
		this->samples_played += samples_played;
		// Replace sd_buf data after sd_p with Frame Data in 'position'

		// [Method 1]: just replace data after sd_p with Frame Data in 'position'
		//fread(sd_p, sd_left);

		// [Method 2]: set sd_p = sd_buf and fill sd_buf fully with Frame Data in 'position'
		sd_left = fillReadBuffer(sd_buf, sd_p, 0, MP3_SD_BUF_SIZE);
		sd_p = sd_buf;
	}

	if((mp3FrameInfo.samprate != AUDIOCODECS_SAMPLE_RATE ) || (mp3FrameInfo.bitsPerSample != 16) || (mp3FrameInfo.nChans > 2)) {
		char str[256];
		sprintf(str, "incompatible MP3 file. samprate: %d, bitsPerSample: %d, nChans: %d", mp3FrameInfo.samprate, mp3FrameInfo.bitsPerSample, mp3FrameInfo.nChans);
		Serial.println(str);
		lastError = ERR_CODEC_FORMAT;
		stop();
		return lastError;
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
void AudioPlaySdMp3::update(void)
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
			Serial.println("[play_sd_mp3] Swap block for rescue");
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

	if (mp3FrameInfo.nChans == 2) {
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

void decodeMp3(void)
{
	codec_event.trigger();
}

//decoding-interrupt
//__attribute__ ((optimize("O2"))) <- does not work here, bug in g++
void decodeMp3_core(void)
{
	if (mp3objptr == NULL) return;
	AudioPlaySdMp3 *o = mp3objptr;
	int db = o->decoding_block;

	/*
	Serial.print("decodeMp3 called: ");
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
			o->sd_left = o->fillReadBuffer(o->sd_buf, o->sd_p, o->sd_left, MP3_SD_BUF_SIZE);
			if (!o->sd_left) { eof = true; goto mp3end; }
			o->sd_p = o->sd_buf;

			uint32_t cycles_rd = (ARM_DWT_CYCCNT - cycles);
			if (cycles_rd > o->decode_cycles_max_read )o-> decode_cycles_max_read = cycles_rd;
			break;
		}

	case 1:
		{
			// find start of next MP3 frame until EOF
			int offset = 0;
			while (1) {
				if (!o->sd_left) {
					//Serial.println("No sync"); //no error at end of file
					eof = true;
					goto mp3end;
				}
				offset = MP3FindSyncWord(o->sd_p, o->sd_left);
				if (offset >= 0) {
					o->sd_p += offset;
					o->sd_left -= offset;
					// Fill Buffer for enough sd_left bytes even through offset is not zero (doubt for previous incoplete frame)
					o->sd_left = o->fillReadBuffer(o->sd_buf, o->sd_p, o->sd_left, MP3_SD_BUF_SIZE);
					o->sd_p = o->sd_buf;
					break;
				}
				o->sd_left = o->fillReadBuffer(o->sd_buf, o->sd_p, 0, MP3_SD_BUF_SIZE);
				o->sd_p = o->sd_buf;
			}

			int decode_res = MP3Decode(o->hMP3Decoder, &o->sd_p, (int*)&o->sd_left,o->buf[db], 0);

			switch(decode_res)
			{
				case ERR_MP3_NONE:
				{
					o->err_cnt = 0;
					MP3GetLastFrameInfo(o->hMP3Decoder, &o->mp3FrameInfo);
					o->decoded_length[db] = o->mp3FrameInfo.outputSamps;
					o->bitrate = o->mp3FrameInfo.bitrate / 1000; // bps -> Kbps
					break;
				}

				//case ERR_MP3_MAINDATA_UNDERFLOW: // Ignore Error for MP3 File generated by mp3DirectCut etc
				//case ERR_MP3_INVALID_HUFFCODES: // Ignore Error for initial dummy frames
				default :
				{
					if (o->err_cnt++ < 1) { // Ignore Error
						MP3GetLastFrameInfo(o->hMP3Decoder, &o->mp3FrameInfo);
						o->decoded_length[db] = o->mp3FrameInfo.outputSamps;
						o->bitrate = o->mp3FrameInfo.bitrate / 1000; // bps -> Kbps
					} else {
						Serial.print("ERROR: mp3 decode_res: ");
						Serial.println(decode_res);
						AudioPlaySdMp3::lastError = decode_res;
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

mp3end:

	o->decoding_state++;
	if (o->decoding_state >= DECODE_NUM_STATES) o->decoding_state = 0;

	if (eof) {
		//o->stop_for_next();
		o->stop();
	}

	/*
	Serial.print("decodeMp3 mp3end: ");
	Serial.println(millis());
	*/
}

// lengthMillis (Override)
unsigned AudioPlaySdMp3::lengthMillis(void)
{
	if (mp3FrameInfo.numFrames) { // also safe for VBR case
		// numFrames - 1: Sub a frame where Xing Header exists
		return ((unsigned long long) (mp3FrameInfo.numFrames - 1) * mp3FrameInfo.nChans * 576 * 1000 / AUDIO_SAMPLE_RATE_EXACT);
	} else {
		return max((fsize() - size_id3) * 8 / bitrate,  positionMillis());
	}
}
