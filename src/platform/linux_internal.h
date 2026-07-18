#ifndef L2DCAT_LINUX_INTERNAL_H
#define L2DCAT_LINUX_INTERNAL_H

#include "l2dcat/platform.h"

bool l2dcat_linux_x11_start(L2DCatPlatform *platform, L2DCatError *error);
void l2dcat_linux_x11_stop(L2DCatPlatform *platform);
bool l2dcat_linux_x11_supported(const L2DCatPlatform *platform);
void l2dcat_linux_x11_click_through(L2DCatPlatform *platform, bool enabled);
void l2dcat_linux_x11_taskbar(L2DCatPlatform *platform, bool visible);
void l2dcat_linux_x11_begin_drag(L2DCatPlatform *platform);
L2DCatMenuAction l2dcat_linux_context_menu(L2DCatPlatform *platform,
    const L2DCatMenuLabels *labels);

#endif
