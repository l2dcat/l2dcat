#ifndef BONGO_CAT_NEO_AUDIO_H
#define BONGO_CAT_NEO_AUDIO_H

#include "bongo_cat_neo/common.h"

typedef struct BongoCatNeoAudio BongoCatNeoAudio;

BongoCatNeoAudio *bongo_cat_neo_audio_create(BongoCatNeoError *error);
void bongo_cat_neo_audio_destroy(BongoCatNeoAudio *audio);
BongoCatNeoResult bongo_cat_neo_audio_play(BongoCatNeoAudio *audio, const char *path, BongoCatNeoError *error);
void bongo_cat_neo_audio_stop(BongoCatNeoAudio *audio);
void bongo_cat_neo_audio_set_enabled(BongoCatNeoAudio *audio, bool enabled);

#endif
