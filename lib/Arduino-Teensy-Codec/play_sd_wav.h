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
 6790´tzui#
	Sie sollten eine Kopie der GNU General Public License zusammen mit diesem
	Programm erhalten haben. Wenn nicht, siehe <http://www.gnu.org/licenses/>.

	Der Helixdecoder selbst hat eine eigene Lizenz, bitte fur mehr Informationen
	in den Unterverzeichnissen nachsehen.

 */

 /* The Helix-Library is modified for Teensy 3.1 */

#ifndef play_sd_wav_h_
#define play_sd_wav_h_

#include "codecs.h"
#include "AudioStream.h"
//#include "spi_interrupt.h"

//#define DEBUG_PLAY_SD_WAV

class AudioPlaySdWav : public AudioCodec
{
public:
	void stop(void);
	//int standby_play(MutexFsBaseFile *file);
	int play(MutexFsBaseFile *file, size_t position = 0, unsigned samples_played = 0) {stop();if (!fopen(file)) return ERR_CODEC_FILE_NOT_FOUND; return play(position, samples_played);}
	//int play(const char *filename) {stop();if (!fopen(filename)) return ERR_CODEC_FILE_NOT_FOUND; return play();}
	//int play(const size_t p, const size_t size) {stop();if (!fopen(p,size)) return ERR_CODEC_FILE_NOT_FOUND; return play();}
	//int play(const uint8_t*p, const size_t size) {stop();if (!fopen(p,size))  return ERR_CODEC_FILE_NOT_FOUND; return play();}
	unsigned lengthMillis(void);
	unsigned positionMillis(void);
	unsigned int parseHeader(MutexFsBaseFile *file);
	size_t fposition(void) { return AudioCodec::fposition() - sd_left; }

protected:
	uint8_t			*sd_buf;
	uint8_t			*sd_p;
	int				sd_left;

	int32_t			*buf[2];
	size_t			decoded_length[2];
	size_t			decoding_block;
	unsigned int	decoding_state; //state 0: read sd, state 1: decode

	uintptr_t 		play_pos;

	unsigned int	samprate;
	unsigned short	bitsPerSample;
	size_t			data_size;

	//void stop_for_next(void);
	size_t parseFmtChunk(uint8_t *sd_buf, size_t sd_buf_size);
	int play(size_t position = 0, unsigned samples_played = 0);
	void update(void);
	friend void decodeWav_core(void);
};

void decodeWav_core(void);

#endif
