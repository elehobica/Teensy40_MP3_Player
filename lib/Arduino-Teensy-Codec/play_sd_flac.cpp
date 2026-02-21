/*
	Arduino Audiocodecs

	Copyright (c) 2014 Frank Bösing

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


	Diese Bibliothek ist freie Software: Sie können es unter den Bedingungen
	der GNU General Public License, wie von der Free Software Foundation,
	Version 3 der Lizenz oder (nach Ihrer Wahl) jeder neueren
	veröffentlichten Version, weiterverbreiten und/oder modifizieren.

	Diese Bibliothek wird in der Hoffnung, dass es nützlich sein wird, aber
	OHNE JEDE GEWÄHRLEISTUNG, bereitgestellt; sogar ohne die implizite
	Gewährleistung der MARKTFÄHIGKEIT oder EIGNUNG FÜR EINEN BESTIMMTEN ZWECK.
	Siehe die GNU General Public License für weitere Details.

	Sie sollten eine Kopie der GNU General Public License zusammen mit diesem
	Programm erhalten haben. Wenn nicht, siehe <http://www.gnu.org/licenses/>.

 */

// Modified by Elehobica for Teensy 4.0 MP3 Player

#include "play_sd_flac.h"
#include <TeensyThreads.h>

extern Threads::Event codec_event;

FLAC__StreamDecoderReadStatus read_callback(const FLAC__StreamDecoder *decoder, FLAC__byte buffer[], size_t *bytes, void *client_data);
FLAC__StreamDecoderWriteStatus write_callback(const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 *const buffer[], void *client_data);
FLAC__StreamDecoderSeekStatus seek_callback(const FLAC__StreamDecoder *decoder, FLAC__uint64 absolute_byte_offset, void *client_data);
FLAC__StreamDecoderTellStatus tell_callback(const FLAC__StreamDecoder *decoder, FLAC__uint64 *absolute_byte_offset, void *client_data);
FLAC__StreamDecoderLengthStatus length_callback(const FLAC__StreamDecoder *decoder, FLAC__uint64 *stream_length, void *client_data);
FLAC__bool eof_callback(const FLAC__StreamDecoder *decoder, void *client_data);
void error_callback(const FLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status, void *client_data);

FLAC__StreamDecoder	*AudioPlaySdFlac::hFLACDecoder;
int AudioPlaySdFlac::run = 0;

AudioPlaySdFlac *flacobjptr;

static bool isValidSampleRate(unsigned int sr) {
	return (sr == 44100 || sr == 48000 || sr == 88200 ||
	        sr == 96000 || sr == 176400 || sr == 192000);
}

void decodeFlac(void);

void AudioPlaySdFlac::stop(void)
{
	NVIC_DISABLE_IRQ(IRQ_AUDIOCODEC);

	playing = codec_stopped;
	run = 0;
	flacobjptr = NULL;

	// Wait for codec_thread to exit write_callback's wait loop
	// and return from FLAC__stream_decoder_process_single()
	for (int i = 0; i < 10; i++) {
		threads.yield();
		delay(5);
	}

	if (audiobuffer != NULL) {
		delete audiobuffer;
		audiobuffer = NULL;
	}
	if (hFLACDecoder != NULL)
	{
		FLAC__stream_decoder_finish(hFLACDecoder);
		FLAC__stream_decoder_delete(hFLACDecoder);
		hFLACDecoder=NULL;
	};
	fclose();
}

#if 0
int AudioPlaySdFlac::standby_play(MutexFsBaseFile *file)
{
	while (isPlaying()) { /*delay(1);*/ } // Wait for previous FLAC playing to stop

	initVars();
	lastError = ERR_CODEC_NONE;
	FLAC__stream_decoder_delete(hFLACDecoder);
	hFLACDecoder = FLAC__stream_decoder_new();
	//FLAC__stream_decoder_reset(hFLACDecoder);
	if (!hFLACDecoder)
	{
		lastError = ERR_CODEC_OUT_OF_MEMORY;
		goto PlayErr;
	}

	//audiobuffer = new AudioBuffer();

	FLAC__StreamDecoderInitStatus ret;
	ret = FLAC__stream_decoder_init_stream(hFLACDecoder,
		read_callback,
		seek_callback,
		tell_callback,
		length_callback,
		eof_callback,
		write_callback,
		NULL,
		error_callback,
		this);

	if (ret != FLAC__STREAM_DECODER_INIT_STATUS_OK)
	{
		if (ret == FLAC__STREAM_DECODER_INIT_STATUS_MEMORY_ALLOCATION_ERROR) lastError = ERR_CODEC_OUT_OF_MEMORY;
		else lastError = ERR_CODEC_FORMAT;
		//Serial.printf("Init: %s",FLAC__StreamDecoderInitStatusString[ret]);
		goto PlayErr;
	}

	if (!FLAC__stream_decoder_process_until_end_of_metadata(hFLACDecoder))
	{
		lastError = ERR_CODEC_FORMAT;
		goto PlayErr;
	}

#ifdef FLAC_USE_SWI
	_VectorsRam[IRQ_AUDIOCODEC + 16] = &decodeFlac;
	initSwi();
#endif

	playing = codec_playing;

    return lastError;

PlayErr:
	stop();
	return lastError;
}
#endif

/*
unsigned AudioPlaySdFlac::sampleRate(void){
	if (hFLACDecoder != NULL)
		return  FLAC__stream_decoder_get_total_samples(hFLACDecoder);
	else
		return 0;
}
*/
/*
int AudioPlaySdFlac::play(const char *filename){
	stop();
	if (!fopen(filename))
		return ERR_CODEC_FILE_NOT_FOUND;
	return initPlay();
}

int AudioPlaySdFlac::play(const size_t p, const size_t size){
	stop();
	if (!fopen(p,size))
		return ERR_CODEC_FILE_NOT_FOUND;
	return initPlay();
}
*/

int AudioPlaySdFlac::play(size_t position, unsigned samples_played)
{
	lastError = ERR_CODEC_NONE;
	initVars();
	samprate = AUDIOCODECS_SAMPLE_RATE;
	bitsPerSample = 16;

	hFLACDecoder = FLAC__stream_decoder_new();
	if (!hFLACDecoder) return ERR_CODEC_OUT_OF_MEMORY;

	flacobjptr = this;

	audiobuffer = new AudioBuffer();

	FLAC__StreamDecoderInitStatus ret;
	ret = FLAC__stream_decoder_init_stream(hFLACDecoder,
		read_callback,
		seek_callback,
		tell_callback,
		length_callback,
		eof_callback,
		write_callback,
		NULL,
		error_callback,
		this);

	if (ret != FLAC__STREAM_DECODER_INIT_STATUS_OK)
	{
		if (ret == FLAC__STREAM_DECODER_INIT_STATUS_MEMORY_ALLOCATION_ERROR) lastError = ERR_CODEC_OUT_OF_MEMORY;
		else lastError = ERR_CODEC_FORMAT;
		//Serial.printf("Init: %s",FLAC__StreamDecoderInitStatusString[ret]);
		goto PlayErr;
	}

	if (!FLAC__stream_decoder_process_until_end_of_metadata(hFLACDecoder))
	{
		lastError = ERR_CODEC_FORMAT;
		goto PlayErr;
	}

#ifdef FLAC_USE_SWI
	_VectorsRam[IRQ_AUDIOCODEC + 16] = &decodeFlac;
	initSwi();
#endif

	// Pre-fill buffer before starting playback (like WAV's double-buffer fill)
	decodeFlac_core();  // Decode first frame (also allocates audiobuffer)
	if (audiobuffer != NULL && audiobuffer->getBufsize() > 0 && audiobuffer->available() >= minbuffers) {
		decodeFlac_core();  // Fill remaining buffer for small-block files
	}

	// For Resume play with 'position'
	if (position != 0 && position < fsize()) {
		FLAC__stream_decoder_seek_absolute(hFLACDecoder, samples_played);
		this->samples_played += samples_played;
	}

	_channels = FLAC__stream_decoder_get_channels(hFLACDecoder);
	samprate = FLAC__stream_decoder_get_sample_rate(hFLACDecoder);
	bitsPerSample = FLAC__stream_decoder_get_bits_per_sample(hFLACDecoder);
	bitrate = (unsigned short) ((unsigned long) samprate * bitsPerSample * _channels / 1000);

	if (!isValidSampleRate(samprate) || _channels > 2 ||
		(bitsPerSample != 16 && bitsPerSample != 24)) {
		Serial.println("incompatible FLAC file.");
		lastError = ERR_CODEC_FORMAT;
		stop();
		return lastError;
	}

	playing = codec_playing;

    return lastError;

PlayErr:
	stop();
	return lastError;
}

__attribute__ ((optimize("O2")))
FLAC__StreamDecoderReadStatus read_callback(const FLAC__StreamDecoder *decoder, FLAC__byte buffer[], size_t *bytes, void *client_data)
{
	uint32_t cycles = ARM_DWT_CYCCNT;
	FLAC__StreamDecoderReadStatus ret = FLAC__STREAM_DECODER_READ_STATUS_ABORT;
	if(*bytes > 0)
	{
		int num = ((AudioPlaySdFlac*)client_data)->fread(buffer, *bytes);
		if (num > 0)
		{
			ret = FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
			*bytes = num;
		}  else
		if(num == 0)
		{
			ret = FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
			*bytes = 0;
		}
		else *bytes = 0;
	}

	((AudioPlaySdFlac*)client_data)->decode_cycles_read += (ARM_DWT_CYCCNT - cycles);
	return ret;
}

/**
 * Seek callback. Called when decoder needs to seek the stream.
 *
 * @param decoder               Decoder instance
 * @param absolute_byte_offset  Offset from beginning of stream to seek to
 * @param client_data           Client data set at initialisation
 *
 * @return Seek status
 */
FLAC__StreamDecoderSeekStatus seek_callback(const FLAC__StreamDecoder* decoder, FLAC__uint64 absolute_byte_offset, void* client_data)
{
	if (!((AudioPlaySdFlac*)client_data)->fseek(absolute_byte_offset)) {
        // seek failed
        return FLAC__STREAM_DECODER_SEEK_STATUS_ERROR;
    }
    else {
        return FLAC__STREAM_DECODER_SEEK_STATUS_OK;
    }
}
/**
 * Tell callback. Called when decoder wants to know current position of stream.
 *
 * @param decoder               Decoder instance
 * @param absolute_byte_offset  Offset from beginning of stream to seek to
 * @param client_data           Client data set at initialisation
 *
 * @return Tell status
 */
FLAC__StreamDecoderTellStatus tell_callback(const FLAC__StreamDecoder* decoder, FLAC__uint64* absolute_byte_offset, void* client_data)
{
    // update offset
    *absolute_byte_offset = (FLAC__uint64)((AudioPlaySdFlac*)client_data)->fposition();
    return FLAC__STREAM_DECODER_TELL_STATUS_OK;
}

/**
 * Length callback. Called when decoder wants total length of stream.
 *
 * @param decoder        Decoder instance
 * @param stream_length  Total length of stream in bytes
 * @param client_data    Client data set at initialisation
 *
 * @return Length status
 */
FLAC__StreamDecoderLengthStatus length_callback(const FLAC__StreamDecoder* decoder, FLAC__uint64* stream_length, void* client_data)
{

    *stream_length = (FLAC__uint64)((AudioPlaySdFlac*)client_data)->fsize();
    return FLAC__STREAM_DECODER_LENGTH_STATUS_OK;

}

/**
 * EOF callback. Called when decoder wants to know if end of stream is reached.
 *
 * @param decoder       Decoder instance
 * @param client_data   Client data set at initialisation
 *
 * @return True if end of stream
 */
FLAC__bool eof_callback(const FLAC__StreamDecoder* decoder, void* client_data)
{
    return ((AudioPlaySdFlac*)client_data)->f_eof();
}
/**
 * Error callback. Called when error occured during decoding.
 *
 * @param decoder       Decoder instance
 * @param status        Error
 * @param client_data   Client data set at initialisation
 */

void error_callback(const FLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status, void *client_data)
{
#ifdef DEBUG_PLAY_SD_FLAC
	Serial.print("[FLAC] error_callback: ");
	Serial.println(FLAC__StreamDecoderErrorStatusString[status]);
#endif
}

/**
 * Write callback. Called when decoder has decoded a single audio frame.
 *
 * @param decoder       Decoder instance
 * @param frame         Decoded frame
 * @param buffer        Array of pointers to decoded channels of data
 * @param client_data   Client data set at initialisation
 *
 * @return Read status
 */
__attribute__ ((optimize("O3")))
FLAC__StreamDecoderWriteStatus write_callback(const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 *const buffer[], void *client_data)
{
	AudioPlaySdFlac *obj = (AudioPlaySdFlac*) client_data;
	int blocksize = frame->header.blocksize  & ~(AUDIO_BLOCK_SAMPLES-1) ;
	int chan = frame->header.channels;
	int bps = frame->header.bits_per_sample;
	size_t numbuffers = (blocksize * chan) / AUDIO_BLOCK_SAMPLES;

	if (obj->audiobuffer->getBufsize() == 0)
	{ //It is our very first frame.
		obj->_channels = chan;
		obj->audiobuffer->allocMem(FLAC_BUFFERS(numbuffers));
		obj->minbuffers	= numbuffers;
	}

	if (chan==0 || chan> 2 ||
		blocksize < AUDIO_BLOCK_SAMPLES
		)
		return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;

	// Wait for ISR to consume enough buffer data instead of aborting
	// (ABORT would discard the decoded frame, causing skipped audio)
	while (obj->audiobuffer->available() < numbuffers) {
		if (obj->playing == codec_stopped) {
			return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
		}
		threads.yield();
	}

	//Copy all the data to the fifo. All samples are normalized to 24-bit in int32_t.
	const FLAC__int32 *sbuf;
	const FLAC__int32 *k;
	int shift = 24 - bps; // 16bit: shift=8, 24bit: shift=0
	int j = 0;
	do
	{
		int i = 0;
		do
		{
			sbuf = &buffer[i][j];
			k = sbuf + AUDIO_BLOCK_SAMPLES;
			int32_t *abufPtr = obj->audiobuffer->alloc();
			if (shift >= 0) {
				do
				{
					*abufPtr++ = (int32_t)(*sbuf++) << shift;
				} while (sbuf < k);
			} else {
				int rshift = -shift;
				do
				{
					*abufPtr++ = (int32_t)(*sbuf++) >> rshift;
				} while (sbuf < k);
			}
		} while (++i < chan);

		j+=	AUDIO_BLOCK_SAMPLES;
	} while (j < blocksize);

	return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}



//runs in ISR
__attribute__ ((optimize("O2")))
void AudioPlaySdFlac::update(void)
{
	audio_block_t	*audioblockL;
	audio_block_t	*audioblockR;

	//paused or stopped ?

	if (playing != codec_playing) return;

	if (_channels > 0 && audiobuffer->used() >= _channels)
	{
		audioblockL = allocate();
		if (!audioblockL) return;
		if (_channels == 2)
		{
			audioblockR = allocate();
			if (!audioblockR) {
				release(audioblockL);
				return;
			}
			int32_t *abufptrL = audiobuffer->get();
			int32_t *abufptrR = audiobuffer->get();
			for (int j=0; j < AUDIO_BLOCK_SAMPLES; j++)
			{
				audioblockL->data[j] = *abufptrL++;
				audioblockR->data[j] = *abufptrR++;
			}
			transmit(audioblockL, 0);
			transmit(audioblockR, 1);
			release(audioblockL);
			release(audioblockR);
		} else
		{
			int32_t *abufptrL = audiobuffer->get();
			for (int j=0; j < AUDIO_BLOCK_SAMPLES; j++)
			{
				audioblockL->data[j] = *abufptrL++;
			}
			transmit(audioblockL, 0);
			transmit(audioblockL, 1);
			release(audioblockL);
		}

		samples_played += AUDIO_BLOCK_SAMPLES;
	} else
	{ //Stop playing
#ifdef FLAC_USE_SWI
		//if (!NVIC_IS_ACTIVE(IRQ_AUDIOCODEC))
#endif
		/*
		{
			FLAC__StreamDecoderState state;
			state = FLAC__stream_decoder_get_state(hFLACDecoder);
			if (state == FLAC__STREAM_DECODER_END_OF_STREAM ||
				state == FLAC__STREAM_DECODER_ABORTED ||
				state == FLAC__STREAM_DECODER_MEMORY_ALLOCATION_ERROR)
				{
					//stop_for_next();
					stop();
					return;
				}
		}
		*/
	}

	if (audiobuffer->getBufsize() > 0 && audiobuffer->available() < minbuffers) return;

#ifndef FLAC_USE_SWI

	decode_cycles_sd = 0;
	FLAC__stream_decoder_process_single(hFLACDecoder);
	if (decode_cycles_sd > decode_cycles_max_sd ) decode_cycles_max_sd = decode_cycles_sd;

#else
	//to give the user-sketch some cpu-time, only chain
	//if the swi is not active currently.
	if (!NVIC_IS_ACTIVE(IRQ_AUDIOCODEC))
			NVIC_TRIGGER_INTERRUPT(IRQ_AUDIOCODEC);
#endif
}

void decodeFlac(void)
{
	codec_event.trigger();
}

void decodeFlac_core_half(void)
{
	AudioPlaySdFlac::run = 1 - AudioPlaySdFlac::run;
	if (AudioPlaySdFlac::run) decodeFlac_core();
}

void decodeFlac_core(void)
{
	if (flacobjptr == NULL) return;
	AudioPlaySdFlac *o = flacobjptr;

	//if (!audiobuffer.available()) return;
	AudioPlaySdFlac::decode_cycles_read = 0;

	uint32_t cycles = ARM_DWT_CYCCNT;

	if (AudioPlaySdFlac::hFLACDecoder == NULL) return;
	FLAC__stream_decoder_process_single(AudioPlaySdFlac::hFLACDecoder);

	cycles = (ARM_DWT_CYCCNT - cycles);
	if (cycles - AudioPlaySdFlac::decode_cycles > AudioPlaySdFlac::decode_cycles_max )
			AudioPlaySdFlac::decode_cycles_max = cycles - AudioPlaySdFlac::decode_cycles;
	if (AudioPlaySdFlac::decode_cycles_read > AudioPlaySdFlac::decode_cycles_max_read )
			AudioPlaySdFlac::decode_cycles_max_read = AudioPlaySdFlac::decode_cycles_read;

	if (FLAC__stream_decoder_get_state(AudioPlaySdFlac::hFLACDecoder) == FLAC__STREAM_DECODER_END_OF_STREAM) {
		o->stop();
	}
}

#if 0
void AudioPlaySdFlac::stop_for_next(void)
{
	Serial.println("stop_for_next");

	//NVIC_DISABLE_IRQ(IRQ_AUDIOCODEC);

	playing = codec_stopped;
	/*
	delete audiobuffer;
	audiobuffer = NULL;
	if (hFLACDecoder != NULL)
	{
		FLAC__stream_decoder_finish(hFLACDecoder);
		FLAC__stream_decoder_delete(hFLACDecoder);
		hFLACDecoder=NULL;
	};
	*/
	fclose();
	hFLACDecoder = NULL;
}
#endif

unsigned AudioPlaySdFlac::positionMillis(void)
{
	return (samprate > 0) ? (unsigned)((uint64_t)samples_played * 1000 / samprate) : 0;
}

unsigned AudioPlaySdFlac::lengthMillis(void)
{
	if (hFLACDecoder != NULL && samprate > 0)
		return ((uint64_t) FLAC__stream_decoder_get_total_samples(hFLACDecoder) * 1000 / samprate);
	else
		return 0;
}

unsigned int AudioPlaySdFlac::parseHeader(MutexFsBaseFile *file)
{
	uint8_t buf[42];
	uint64_t saved_pos = file->curPosition();
	file->seekSet(0);
	int bytes_read = file->read(buf, sizeof(buf));
	file->seekSet(saved_pos);
	if (bytes_read < 42) return 0;
	// Check "fLaC" marker
	if (buf[0]!='f' || buf[1]!='L' || buf[2]!='a' || buf[3]!='C') return 0;
	// Check STREAMINFO block type (lower 7 bits should be 0)
	if ((buf[4] & 0x7F) != 0) return 0;
	// STREAMINFO data starts at buf[8]
	uint8_t *si = buf + 8;
	samprate = ((unsigned int)si[10] << 12) | ((unsigned int)si[11] << 4) | (si[12] >> 4);
	bitsPerSample = ((si[12] & 1) << 4 | (si[13] >> 4)) + 1;
	return samprate;
}
