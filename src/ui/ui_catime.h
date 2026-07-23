#ifndef L2DCAT_UI_CATIME_H
#define L2DCAT_UI_CATIME_H

#include <stdbool.h>
#include "nuklear_config.h"

#define L2DCAT_UI_MARGIN 10.0f
#define L2DCAT_UI_HEADER_HEIGHT 76.0f
#define L2DCAT_UI_TABS_HEIGHT 58.0f

typedef struct L2DCatUIPalette {
    struct nk_color background;
    struct nk_color surface;
    struct nk_color field;
    struct nk_color border;
    struct nk_color text;
    struct nk_color muted;
    struct nk_color accent;
    struct nk_color accent_hover;
    struct nk_color accent_pressed;
    struct nk_color hover;
    struct nk_color selection;
    struct nk_color danger;
    struct nk_color danger_background;
} L2DCatUIPalette;

L2DCatUIPalette l2dcat_ui_palette(bool dark);
bool l2dcat_ui_dark(const struct nk_context *context);
void l2dcat_ui_apply_theme(struct nk_context *context, bool dark);
void l2dcat_ui_shell_draw(struct nk_context *context, float width,
    float height, bool dark);
bool l2dcat_ui_header(struct nk_context *context, const char *title,
    const struct nk_user_font *font, bool interactive, bool dark);
void l2dcat_ui_tabs(struct nk_context *context, const char *const *labels,
    int count, int *active, bool interactive, bool dark);
bool l2dcat_ui_close_hit(float x, float y, float width);
bool l2dcat_ui_title_drag_hit(float x, float y, float width);

#endif
