/*------------------------------------------------------/
/ audio_playback
/-------------------------------------------------------/
/ Copyright (c) 2020, Elehobica
/ Released under the BSD-2-Clause
/ refer to https://opensource.org/licenses/BSD-2-Clause
/------------------------------------------------------*/

// Modified from following sample code
//   https://github.com/FrankBoesing/Arduino-Teensy-Codec-lib
//
// This example code is in the public domain.

#ifndef __AUDIO_PLAYBACK_H__
#define __AUDIO_PLAYBACK_H__

#include <stdint.h>
#include <SdFat.h>
#include <codecs.h>

typedef enum _audio_codec_enm_t {
    CodecMp3 = 0,
    CodecWav,
    CodecAac,
    CodecFlac,
    CodecNone
} audio_codec_enm_t;

void audio_init();
void audio_set_codec(audio_codec_enm_t audio_codec_enm);
AudioCodec *audio_get_codec();
void audio_set_volume(uint8_t value);
uint8_t audio_get_volume();
uint8_t audio_get_last_volume();
void audio_volume_up();
void audio_volume_down();
void audio_play(MutexFsBaseFile *file);
void audio_terminate();
void audio_set_position(size_t fpos, uint32_t samples_played);
void audio_get_position(size_t *fpos, uint32_t *samples_played);
void audio_i2s_mute(bool mute);
void audio_spdif_mute(bool mute);

#endif // __AUDIO_PLAYBACK_H__