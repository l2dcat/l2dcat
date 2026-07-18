#ifndef L2DCAT_MACOS_INTERNAL_H
#define L2DCAT_MACOS_INTERNAL_H

#include "l2dcat/platform.h"

bool l2dcat_macos_input_start(L2DCatPlatform *platform, L2DCatError *error);
void l2dcat_macos_input_stop(L2DCatPlatform *platform);
bool l2dcat_macos_input_supported(void);
L2DCatMenuAction l2dcat_macos_context_menu(L2DCatPlatform *platform,
    const L2DCatMenuLabels *labels);

#endif
