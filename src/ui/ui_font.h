#ifndef BONGO_CAT_NEO_UI_FONT_H
#define BONGO_CAT_NEO_UI_FONT_H

#include <stddef.h>
#include <stdbool.h>

const char *bongo_cat_neo_ui_system_font(char *path, size_t capacity, bool multilingual);
const char *bongo_cat_neo_ui_system_heading_font(char *path, size_t capacity,
    bool multilingual);

#endif
