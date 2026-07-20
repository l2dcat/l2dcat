#include "l2dcat/audio.h"

#include <miniaudio.h>
#include <stdlib.h>
#include <string.h>

struct L2DCatAudio {
    ma_engine engine;
    ma_sound sound;
    bool initialized;
    bool sound_ready;
    bool enabled;
};

L2DCatAudio *l2dcat_audio_create(L2DCatError *error) {
    (void)error;
    L2DCatAudio *audio = calloc(1, sizeof(*audio));
    if (!audio) return NULL;
    audio->enabled = true;
    return audio;
}

void l2dcat_audio_stop(L2DCatAudio *audio) {
    if (!audio || !audio->sound_ready) return;
    ma_sound_stop(&audio->sound);
    ma_sound_uninit(&audio->sound);
    audio->sound_ready = false;
}

L2DCatResult l2dcat_audio_play(L2DCatAudio *audio, const char *path, L2DCatError *error) {
    if (!audio || !path) return L2DCAT_ERROR_ARGUMENT;
    if (!audio->enabled) return L2DCAT_OK;
    if (!audio->initialized) {
        ma_result initialized = ma_engine_init(NULL, &audio->engine);
        if (initialized != MA_SUCCESS) {
            l2dcat_error_set(error, L2DCAT_ERROR_PLATFORM,
                "Audio initialization failed: %d", initialized);
            return L2DCAT_ERROR_PLATFORM;
        }
        audio->initialized = true;
    }
    l2dcat_audio_stop(audio);
    ma_result result = ma_sound_init_from_file(&audio->engine, path,
        MA_SOUND_FLAG_DECODE | MA_SOUND_FLAG_ASYNC, NULL, NULL, &audio->sound);
    if (result != MA_SUCCESS) {
        l2dcat_error_set(error, L2DCAT_ERROR_IO, "Cannot load motion sound: %s", path);
        return L2DCAT_ERROR_IO;
    }
    audio->sound_ready = true;
    result = ma_sound_start(&audio->sound);
    if (result != MA_SUCCESS) {
        l2dcat_audio_stop(audio);
        l2dcat_error_set(error, L2DCAT_ERROR_PLATFORM,
            "Cannot start motion sound: %s", path);
        return L2DCAT_ERROR_PLATFORM;
    }
    return L2DCAT_OK;
}

void l2dcat_audio_set_enabled(L2DCatAudio *audio, bool enabled) {
    if (!audio) return;
    audio->enabled = enabled;
    if (!enabled) l2dcat_audio_stop(audio);
}

void l2dcat_audio_destroy(L2DCatAudio *audio) {
    if (!audio) return;
    l2dcat_audio_stop(audio);
    if (audio->initialized) ma_engine_uninit(&audio->engine);
    free(audio);
}
