#ifndef L2DCAT_UI_THEME_H
#define L2DCAT_UI_THEME_H

#include <stdbool.h>
#include "nuklear_config.h"

void l2dcat_ui_apply_theme(struct nk_context *context, bool dark);
bool l2dcat_ui_nav_button(struct nk_context *context, const char *label,
    bool active, bool dark);

#endif
