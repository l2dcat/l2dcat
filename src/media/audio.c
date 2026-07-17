#include "bongo/audio.h"

#include <miniaudio.h>
#include <stdlib.h>
#include <string.h>

struct BongoAudio {
    ma_engine engine;
    ma_sound sound;
    bool initialized;
    bool sound_ready;
    bool enabled;
};

BongoAudio *bongo_audio_create(BongoError *error) {
    (void)error;
    BongoAudio *audio = calloc(1, sizeof(*audio));
    if (!audio) return NULL;
    audio->enabled = true;
    return audio;
}

void bongo_audio_stop(BongoAudio *audio) {
    if (!audio || !audio->sound_ready) return;
    ma_sound_stop(&audio->sound);
    ma_sound_uninit(&audio->sound);
    audio->sound_ready = false;
}

BongoResult bongo_audio_play(BongoAudio *audio, const char *path, BongoError *error) {
    if (!audio || !path) return BONGO_ERROR_ARGUMENT;
    if (!audio->enabled) return BONGO_OK;
    if (!audio->initialized) {
        ma_result initialized = ma_engine_init(NULL, &audio->engine);
        if (initialized != MA_SUCCESS) {
            bongo_error_set(error, BONGO_ERROR_PLATFORM,
                "Audio initialization failed: %d", initialized);
            return BONGO_ERROR_PLATFORM;
        }
        audio->initialized = true;
    }
    bongo_audio_stop(audio);
    ma_result result = ma_sound_init_from_file(&audio->engine, path,
        MA_SOUND_FLAG_DECODE | MA_SOUND_FLAG_ASYNC, NULL, NULL, &audio->sound);
    if (result != MA_SUCCESS) {
        bongo_error_set(error, BONGO_ERROR_IO, "Cannot load motion sound: %s", path);
        return BONGO_ERROR_IO;
    }
    audio->sound_ready = true;
    ma_sound_start(&audio->sound);
    return BONGO_OK;
}

void bongo_audio_set_enabled(BongoAudio *audio, bool enabled) {
    if (!audio) return;
    audio->enabled = enabled;
    if (!enabled) bongo_audio_stop(audio);
}

void bongo_audio_destroy(BongoAudio *audio) {
    if (!audio) return;
    bongo_audio_stop(audio);
    if (audio->initialized) ma_engine_uninit(&audio->engine);
    free(audio);
}
