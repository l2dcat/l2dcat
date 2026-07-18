#ifndef L2DCAT_UI_SIDEBAR_H
#define L2DCAT_UI_SIDEBAR_H

#include <stdbool.h>
#include "nuklear_config.h"

void l2dcat_ui_sidebar(struct nk_context *context, const char *const labels[5],
    int *active_page, bool dark);

#endif
