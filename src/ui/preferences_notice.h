#ifndef BONGO_CAT_NEO_PREFERENCES_NOTICE_H
#define BONGO_CAT_NEO_PREFERENCES_NOTICE_H

#include "bongo_cat_neo/app.h"
#include "bongo_cat_neo/preferences.h"
#include "nuklear_config.h"

void bongo_cat_neo_preferences_notice_show(BongoCatNeoApp *app,
    const char *message, bool error);
void bongo_cat_neo_preferences_notice_draw(BongoCatNeoPreferences *preferences,
    struct nk_context *context, float width, float height);

#endif
