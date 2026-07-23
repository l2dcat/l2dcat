#include "bongo_cat_neo/audio.h"

#include <miniaudio.h>
#include <stdlib.h>
#include <string.h>

struct BongoCatNeoAudio {
    ma_engine engine;
    ma_sound sound;
    bool initialized;
    bool sound_ready;
    bool enabled;
};

BongoCatNeoAudio *bongo_cat_neo_audio_create(BongoCatNeoError *error) {
    (void)error;
    BongoCatNeoAudio *audio = calloc(1, sizeof(*audio));
    if (!audio) return NULL;
    audio->enabled = true;
    return audio;
}

void bongo_cat_neo_audio_stop(BongoCatNeoAudio *audio) {
    if (!audio || !audio->sound_ready) return;
    ma_sound_stop(&audio->sound);
    ma_sound_uninit(&audio->sound);
    audio->sound_ready = false;
}

BongoCatNeoResult bongo_cat_neo_audio_play(BongoCatNeoAudio *audio, const char *path, BongoCatNeoError *error) {
    if (!audio || !path) return BONGO_CAT_NEO_ERROR_ARGUMENT;
    if (!audio->enabled) return BONGO_CAT_NEO_OK;
    if (!audio->initialized) {
        ma_result initialized = ma_engine_init(NULL, &audio->engine);
        if (initialized != MA_SUCCESS) {
            bongo_cat_neo_error_set(error, BONGO_CAT_NEO_ERROR_PLATFORM,
                "Audio initialization failed: %d", initialized);
            return BONGO_CAT_NEO_ERROR_PLATFORM;
        }
        audio->initialized = true;
    }
    bongo_cat_neo_audio_stop(audio);
    ma_result result = ma_sound_init_from_file(&audio->engine, path,
        MA_SOUND_FLAG_DECODE | MA_SOUND_FLAG_ASYNC, NULL, NULL, &audio->sound);
    if (result != MA_SUCCESS) {
        bongo_cat_neo_error_set(error, BONGO_CAT_NEO_ERROR_IO, "Cannot load motion sound: %s", path);
        return BONGO_CAT_NEO_ERROR_IO;
    }
    audio->sound_ready = true;
    result = ma_sound_start(&audio->sound);
    if (result != MA_SUCCESS) {
        bongo_cat_neo_audio_stop(audio);
        bongo_cat_neo_error_set(error, BONGO_CAT_NEO_ERROR_PLATFORM,
            "Cannot start motion sound: %s", path);
        return BONGO_CAT_NEO_ERROR_PLATFORM;
    }
    return BONGO_CAT_NEO_OK;
}

void bongo_cat_neo_audio_set_enabled(BongoCatNeoAudio *audio, bool enabled) {
    if (!audio) return;
    audio->enabled = enabled;
    if (!enabled) bongo_cat_neo_audio_stop(audio);
}

void bongo_cat_neo_audio_destroy(BongoCatNeoAudio *audio) {
    if (!audio) return;
    bongo_cat_neo_audio_stop(audio);
    if (audio->initialized) ma_engine_uninit(&audio->engine);
    free(audio);
}
