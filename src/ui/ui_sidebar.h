#ifndef BONGO_UI_SIDEBAR_H
#define BONGO_UI_SIDEBAR_H

#include <stdbool.h>
#include "nuklear_config.h"

void bongo_ui_sidebar(struct nk_context *context, const char *const labels[5],
    int *active_page, bool dark);

#endif
