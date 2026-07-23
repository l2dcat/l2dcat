#ifndef L2DCAT_UI_FONT_H
#define L2DCAT_UI_FONT_H

#include <stddef.h>
#include <stdbool.h>

const char *l2dcat_ui_system_font(char *path, size_t capacity, bool multilingual);
const char *l2dcat_ui_system_heading_font(char *path, size_t capacity,
    bool multilingual);

#endif
