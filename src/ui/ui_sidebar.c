#include "ui_sidebar.h"
#include "ui_backend.h"

#include <string.h>

static struct nk_color color(bool dark, int light, int night) {
    int value = dark ? night : light;
    return nk_rgb((value >> 16) & 255, (value >> 8) & 255, value & 255);
}

static void cat_icon(struct nk_command_buffer *canvas, float x, float y,
    float size, struct nk_color value) {
    float line = size > 32 ? 2.5f : 2.0f;
    nk_stroke_circle(canvas, nk_rect(x + size * .18f, y + size * .22f,
        size * .64f, size * .58f), line, value);
    nk_stroke_triangle(canvas, x + size * .20f, y + size * .32f,
        x + size * .24f, y + size * .04f, x + size * .43f, y + size * .24f, line, value);
    nk_stroke_triangle(canvas, x + size * .57f, y + size * .24f,
        x + size * .76f, y + size * .04f, x + size * .80f, y + size * .32f, line, value);
    nk_fill_circle(canvas, nk_rect(x + size * .36f, y + size * .43f, 3, 3), value);
    nk_fill_circle(canvas, nk_rect(x + size * .61f, y + size * .43f, 3, 3), value);
}

static void gear_icon(struct nk_command_buffer *canvas, float x, float y,
    float size, struct nk_color value) {
    float cx = x + size * .5f, cy = y + size * .5f;
    nk_stroke_circle(canvas, nk_rect(x + size * .25f, y + size * .25f,
        size * .5f, size * .5f), 2, value);
    nk_stroke_circle(canvas, nk_rect(x + size * .42f, y + size * .42f,
        size * .16f, size * .16f), 2, value);
    for (int i = 0; i < 4; ++i) {
        float dx = i < 2 ? (i ? 1.0f : -1.0f) : 0.0f;
        float dy = i >= 2 ? (i == 2 ? -1.0f : 1.0f) : 0.0f;
        nk_stroke_line(canvas, cx + dx * size * .23f, cy + dy * size * .23f,
            cx + dx * size * .39f, cy + dy * size * .39f, 3, value);
    }
}

static void wand_icon(struct nk_command_buffer *canvas, float x, float y,
    float size, struct nk_color value) {
    nk_stroke_line(canvas, x + size * .20f, y + size * .80f,
        x + size * .72f, y + size * .28f, 3, value);
    float cx = x + size * .76f, cy = y + size * .22f, r = size * .16f;
    nk_stroke_line(canvas, cx - r, cy, cx + r, cy, 2, value);
    nk_stroke_line(canvas, cx, cy - r, cx, cy + r, 2, value);
    nk_stroke_line(canvas, cx - r * .7f, cy - r * .7f,
        cx + r * .7f, cy + r * .7f, 2, value);
    nk_stroke_line(canvas, cx + r * .7f, cy - r * .7f,
        cx - r * .7f, cy + r * .7f, 2, value);
}

static void keyboard_icon(struct nk_command_buffer *canvas, float x, float y,
    float size, struct nk_color value) {
    nk_stroke_rect(canvas, nk_rect(x + size * .12f, y + size * .23f,
        size * .76f, size * .54f), 4, 2, value);
    for (int row = 0; row < 2; ++row)
        for (int col = 0; col < 4; ++col)
            nk_fill_rect(canvas, nk_rect(x + size * (.22f + col * .15f),
                y + size * (.34f + row * .18f), 3, 3), 1, value);
}

static void info_icon(struct nk_command_buffer *canvas, float x, float y,
    float size, struct nk_color value) {
    nk_stroke_circle(canvas, nk_rect(x + size * .18f, y + size * .18f,
        size * .64f, size * .64f), 2, value);
    nk_fill_circle(canvas, nk_rect(x + size * .47f, y + size * .30f, 3, 3), value);
    nk_stroke_line(canvas, x + size * .50f, y + size * .47f,
        x + size * .50f, y + size * .68f, 3, value);
}

static void draw_icon(struct nk_command_buffer *canvas, int index,
    float x, float y, float size, struct nk_color value) {
    if (index == 0) cat_icon(canvas, x, y, size, value);
    else if (index == 1) gear_icon(canvas, x, y, size, value);
    else if (index == 2) wand_icon(canvas, x, y, size, value);
    else if (index == 3) keyboard_icon(canvas, x, y, size, value);
    else info_icon(canvas, x, y, size, value);
}

static bool menu_item(struct nk_context *context, const char *label, int index,
    bool active, bool dark) {
    struct nk_rect bounds;
    nk_layout_row_dynamic(context, 52, 1);
    enum nk_widget_layout_states state = nk_widget(&bounds, context);
    if (state == NK_WIDGET_INVALID) return false;
    bool hover = nk_input_is_mouse_hovering_rect(&context->input, bounds);
    if (hover) l2dcat_ui_cursor_hover_rect(context, bounds,
        L2DCAT_UI_CURSOR_POINTER);
    struct nk_command_buffer *canvas = nk_window_get_canvas(context);
    if (active || hover) nk_fill_rect(canvas, bounds, 12,
        color(dark, active ? 0xE8F1FF : 0xF1F5F9,
            active ? 0x24324A : 0x202735));
    struct nk_color foreground = active ? color(dark, 0x1677FF, 0x3C89E8) :
        color(dark, 0x6B7280, 0xAAB2C0);
    float icon_size = 26;
    draw_icon(canvas, index, bounds.x + 14, bounds.y + 13, icon_size, foreground);
    struct nk_rect text = nk_rect(bounds.x + 52, bounds.y + 15, bounds.w - 62, 24);
    nk_draw_text(canvas, text, label, (int)strlen(label), context->style.font,
        nk_rgba(0, 0, 0, 0), foreground);
    return nk_input_is_mouse_click_in_rect(&context->input, NK_BUTTON_LEFT, bounds);
}

void l2dcat_ui_sidebar(struct nk_context *context, const char *const labels[5],
    int *active_page, bool dark) {
    for (int i = 0; i < 5; ++i)
        if (menu_item(context, labels[i], i, *active_page == i, dark))
            *active_page = i;
}
