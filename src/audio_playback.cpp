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

#include "audio_playback.h"
#include <Audio.h>
#include <play_sd_mp3.h>
#include <play_sd_wav.h>
#include <play_sd_aac.h>
#include <play_sd_flac.h>
#include <output_i2s.h>
#include <output_spdif2.h>
#include <TeensyThreads.h>

AudioPlaySdMp3      playMp3;
AudioPlaySdWav      playWav;
AudioPlaySdAac      playAac;
AudioPlaySdFlac     playFlac;
AudioCodec          *codec = &playMp3;
AudioOutputI2S      i2s1;
AudioOutputSPDIF2   spdif2;
AudioMixer4         mixer0;
AudioMixer4         mixer1;
AudioConnection     patchCordIn0_0(playMp3, 0, mixer0, 0);
AudioConnection     patchCordIn0_1(playMp3, 1, mixer1, 0);
AudioConnection     patchCordIn1_0(playWav, 0, mixer0, 1);
AudioConnection     patchCordIn1_1(playWav, 1, mixer1, 1);
AudioConnection     patchCordIn2_0(playAac, 0, mixer0, 2);
AudioConnection     patchCordIn2_1(playAac, 1, mixer1, 2);
AudioConnection     patchCordIn3_0(playFlac, 0, mixer0, 3);
AudioConnection     patchCordIn3_1(playFlac, 1, mixer1, 3);
AudioConnection     patchCordOut0(mixer0, 0, i2s1, 0);
AudioConnection     patchCordOut1(mixer1, 0, i2s1, 1);
AudioConnection     patchCordSpdifOut0(mixer0, 0, spdif2, 0);
AudioConnection     patchCordSpdifOut1(mixer1, 0, spdif2, 1);

Threads::Event codec_event;

uint8_t last_volume = 65;
size_t _fpos = 0;
uint32_t _samples_played = 0;
static unsigned int current_sample_rate = AUDIOCODECS_SAMPLE_RATE;
static const unsigned int SAMPLE_RATE_SWITCH_SILENCE_MS = 1000;

void codec_thread()
{
    bool wait_timeout = false;
    while (1) {
        if (!codec_event.wait(100)) {
            if (codec->isPlaying() && !wait_timeout) {
                Serial.println("wait timeout");
                wait_timeout = true;
            } else {
                continue;
            }
        } else {
            codec_event.clear();
            wait_timeout = false;
        }
        decodeMp3_core();
        decodeWav_core();
        decodeAac_core_x2();
        decodeFlac_core();
    }
}

void audio_init()
{
    // Audio connections require memory to work.  For more
    // detailed information, see the MemoryAndCpuUsage example
    AudioMemory(15); // 5 for Single MP3
    codec = &playMp3;

    threads.addThread(codec_thread, 1, 2048);
}

void audio_set_codec(audio_codec_enm_t audio_codec_enm)
{
    switch (audio_codec_enm) {
        case CodecMp3:
            codec = &playMp3;
            break;
        case CodecWav:
            codec = &playWav;
            break;
        case CodecAac:
            codec = &playAac;
            break;
        case CodecFlac:
            codec = &playFlac;
            break;
        default:
            break;
    }
}

AudioCodec *audio_get_codec()
{
    return codec;
}

void audio_set_volume(uint8_t value)
{
    i2s1.set_volume(value);
}

uint8_t audio_get_volume()
{
    return i2s1.get_volume();
}

uint8_t audio_get_last_volume()
{
    return last_volume;
}

void audio_volume_up()
{
    i2s1.volume_up();
}

void audio_volume_down()
{
    i2s1.volume_down();
}

void audio_play(MutexFsBaseFile *file)
{
    // Detect sample rate from file header; default to 44100 for unsupported codecs
    unsigned int target_sr = AUDIOCODECS_SAMPLE_RATE;
    if (codec == &playMp3) {
        unsigned int mp3_sr = playMp3.parseHeader(file);
        if (mp3_sr > 0) target_sr = mp3_sr;
    } else if (codec == &playWav) {
        unsigned int wav_sr = playWav.parseHeader(file);
        if (wav_sr > 0) target_sr = wav_sr;
    } else if (codec == &playFlac) {
        unsigned int flac_sr = playFlac.parseHeader(file);
        if (flac_sr > 0) target_sr = flac_sr;
    }
    // Reconfigure I2S/SPDIF clocks if sample rate changed
    if (target_sr != current_sample_rate) {
        audio_i2s_mute(true);
        audio_spdif_mute(true);
        AudioOutputI2S::setFrequency((float)target_sr);
        AudioOutputSPDIF2::setFrequency((float)target_sr);
        delay(1);
        audio_spdif_mute(false);
        audio_i2s_mute(false);
        // Feed silence during DAC fade-in to prevent head clipping
        delay(SAMPLE_RATE_SWITCH_SILENCE_MS);
        current_sample_rate = target_sr;
    }
    codec->play(file, _fpos, _samples_played);
    Serial.printf("[audio] %ubit / %uHz / %dch\r\n", codec->bitResolution(), codec->sampleRate(), codec->channels());
    _fpos = 0;
    _samples_played = 0;
}

void audio_terminate()
{
    _fpos = 0;
    _samples_played = 0;
    last_volume = i2s1.get_volume();
    if (codec->isPlaying()) {
        _fpos = codec->fposition();
        _samples_played = codec->getSamplesPlayed();
        while (i2s1.get_volume() > 0) {
            i2s1.volume_down();
            delay(5);
            yield(); // Arduino msg loop
        }
        codec->stop();
    }
}

void audio_set_position(size_t fpos, uint32_t samples_played)
{
    _fpos = fpos;
    _samples_played = samples_played;
}

void audio_get_position(size_t *fpos, uint32_t *samples_played)
{
    *fpos = _fpos;
    *samples_played = _samples_played;
}

void audio_i2s_mute(bool mute)
{
    if (mute) {
        // Disconnect Pin 7 from SAI1 and drive LOW to stop I2S signal
        pinMode(7, OUTPUT);
        digitalWrite(7, LOW);
    } else {
        // Reconnect Pin 7 to SAI1_TX_DATA0 to resume I2S signal
        CORE_PIN7_CONFIG = 3;
    }
}

void audio_spdif_mute(bool mute)
{
    if (mute) {
        // Disconnect Pin 2 from SAI2 and drive LOW to stop S/PDIF signal
        pinMode(2, OUTPUT);
        digitalWrite(2, LOW);
        // Cut power to S/PDIF TX module (Pin 3)
        pinMode(3, INPUT);
    } else {
        // Supply 3.3V to S/PDIF TX module (Pin 3)
        pinMode(3, OUTPUT);
        digitalWrite(3, HIGH);
        // Reconnect Pin 2 to SAI2_TX_DATA0 to resume S/PDIF signal
        CORE_PIN2_CONFIG = 2;
    }
}