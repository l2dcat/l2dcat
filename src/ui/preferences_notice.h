#ifndef L2DCAT_PREFERENCES_NOTICE_H
#define L2DCAT_PREFERENCES_NOTICE_H

#include "l2dcat/app.h"
#include "l2dcat/preferences.h"
#include "nuklear_config.h"

void l2dcat_preferences_notice_show(L2DCatApp *app,
    const char *message, bool error);
void l2dcat_preferences_notice_draw(L2DCatPreferences *preferences,
    struct nk_context *context, float width, float height);

#endif
