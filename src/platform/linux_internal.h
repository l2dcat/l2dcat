#ifndef BONGO_CAT_NEO_LINUX_INTERNAL_H
#define BONGO_CAT_NEO_LINUX_INTERNAL_H

#include "bongo_cat_neo/platform.h"

bool bongo_cat_neo_linux_x11_start(BongoCatNeoPlatform *platform, BongoCatNeoError *error);
void bongo_cat_neo_linux_x11_stop(BongoCatNeoPlatform *platform);
bool bongo_cat_neo_linux_x11_supported(const BongoCatNeoPlatform *platform);
void bongo_cat_neo_linux_x11_click_through(BongoCatNeoPlatform *platform, bool enabled);
void bongo_cat_neo_linux_x11_taskbar(BongoCatNeoPlatform *platform, bool visible);
void bongo_cat_neo_linux_x11_begin_drag(BongoCatNeoPlatform *platform);
BongoCatNeoMenuAction bongo_cat_neo_linux_context_menu(BongoCatNeoPlatform *platform,
    const BongoCatNeoMenuLabels *labels);

#endif
