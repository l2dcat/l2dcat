#ifndef BONGO_AUDIO_H
#define BONGO_AUDIO_H

#include "bongo/common.h"

typedef struct BongoAudio BongoAudio;

BongoAudio *bongo_audio_create(BongoError *error);
void bongo_audio_destroy(BongoAudio *audio);
BongoResult bongo_audio_play(BongoAudio *audio, const char *path, BongoError *error);
void bongo_audio_stop(BongoAudio *audio);
void bongo_audio_set_enabled(BongoAudio *audio, bool enabled);

#endif
