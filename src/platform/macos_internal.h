#ifndef BONGO_MACOS_INTERNAL_H
#define BONGO_MACOS_INTERNAL_H

#include "bongo/platform.h"

bool bongo_macos_input_start(BongoPlatform *platform, BongoError *error);
void bongo_macos_input_stop(BongoPlatform *platform);
bool bongo_macos_input_supported(void);
BongoMenuAction bongo_macos_context_menu(BongoPlatform *platform,
    const BongoMenuLabels *labels);

#endif
