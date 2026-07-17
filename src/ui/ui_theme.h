#ifndef BONGO_UI_THEME_H
#define BONGO_UI_THEME_H

#include <stdbool.h>
#include "nuklear_config.h"

void bongo_ui_apply_theme(struct nk_context *context, bool dark);
bool bongo_ui_nav_button(struct nk_context *context, const char *label,
    bool active, bool dark);

#endif
