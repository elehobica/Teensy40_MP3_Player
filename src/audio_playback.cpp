#include "audio_playback.h"
#include <Audio.h>
#include <play_sd_mp3.h>
#include <play_sd_wav.h>
#include <play_sd_aac.h>
#include <play_sd_flac.h>
#include <output_i2s.h>
#include <TeensyThreads.h>

AudioPlaySdMp3      playMp3;
AudioPlaySdWav      playWav;
AudioPlaySdAac      playAac;
AudioPlaySdFlac     playFlac;
AudioCodec          *codec = &playMp3;
AudioOutputI2S      i2s1;
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

Threads::Event codec_event;

uint8_t last_volume = 65;
size_t _fpos = 0;
uint32_t _samples_played = 0;

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
        decodeFlac_core_half();
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
    codec->play(file, _fpos, _samples_played);
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