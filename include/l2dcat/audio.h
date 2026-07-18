#ifndef L2DCAT_AUDIO_H
#define L2DCAT_AUDIO_H

#include "l2dcat/common.h"

typedef struct L2DCatAudio L2DCatAudio;

L2DCatAudio *l2dcat_audio_create(L2DCatError *error);
void l2dcat_audio_destroy(L2DCatAudio *audio);
L2DCatResult l2dcat_audio_play(L2DCatAudio *audio, const char *path, L2DCatError *error);
void l2dcat_audio_stop(L2DCatAudio *audio);
void l2dcat_audio_set_enabled(L2DCatAudio *audio, bool enabled);

#endif
