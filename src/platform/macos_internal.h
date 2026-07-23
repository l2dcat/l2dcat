#ifndef BONGO_CAT_NEO_MACOS_INTERNAL_H
#define BONGO_CAT_NEO_MACOS_INTERNAL_H

#include "bongo_cat_neo/platform.h"

bool bongo_cat_neo_macos_input_start(BongoCatNeoPlatform *platform, BongoCatNeoError *error);
void bongo_cat_neo_macos_input_stop(BongoCatNeoPlatform *platform);
bool bongo_cat_neo_macos_input_supported(void);
BongoCatNeoMenuAction bongo_cat_neo_macos_context_menu(BongoCatNeoPlatform *platform,
    const BongoCatNeoMenuLabels *labels);

#endif
