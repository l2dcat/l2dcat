#ifndef BONGO_LINUX_INTERNAL_H
#define BONGO_LINUX_INTERNAL_H

#include "bongo/platform.h"

bool bongo_linux_x11_start(BongoPlatform *platform, BongoError *error);
void bongo_linux_x11_stop(BongoPlatform *platform);
bool bongo_linux_x11_supported(const BongoPlatform *platform);
void bongo_linux_x11_click_through(BongoPlatform *platform, bool enabled);
void bongo_linux_x11_taskbar(BongoPlatform *platform, bool visible);
void bongo_linux_x11_begin_drag(BongoPlatform *platform);
BongoMenuAction bongo_linux_context_menu(BongoPlatform *platform,
    const BongoMenuLabels *labels);

#endif
