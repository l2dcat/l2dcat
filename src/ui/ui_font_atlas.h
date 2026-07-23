#ifndef L2DCAT_UI_FONT_ATLAS_H
#define L2DCAT_UI_FONT_ATLAS_H

#include "ui_backend.h"

bool l2dcat_ui_font_atlas_create(L2DCatUIBackend *ui,
    const char *body_path, const char *heading_path,
    const nk_rune *glyph_ranges);
void l2dcat_ui_font_atlas_destroy(L2DCatUIBackend *ui);

#endif
