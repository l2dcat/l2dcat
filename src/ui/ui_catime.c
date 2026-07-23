#include "ui_catime.h"
#include "ui_backend.h"

static struct nk_color rgb(int value) {
    return nk_rgba((value >> 16) & 255, (value >> 8) & 255, value & 255, 255);
}

static struct nk_color blend(struct nk_color from, struct nk_color to,
    int percent) {
    int keep = 100 - percent;
    return nk_rgba((from.r * keep + to.r * percent) / 100,
        (from.g * keep + to.g * percent) / 100,
        (from.b * keep + to.b * percent) / 100, 255);
}

static float text_width(const struct nk_user_font *font, const char *text) {
    return font && text ? font->width(font->userdata, font->height,
        text, nk_strlen(text)) : 0.0f;
}

static void draw_centered(struct nk_command_buffer *canvas,
    struct nk_rect bounds, const char *text, const struct nk_user_font *font,
    struct nk_color color) {
    float width = text_width(font, text);
    struct nk_rect target = nk_rect(bounds.x + (bounds.w - width) * .5f,
        bounds.y + (bounds.h - font->height) * .5f, width + 1, font->height);
    nk_draw_text(canvas, target, text, nk_strlen(text), font,
        nk_rgba(0, 0, 0, 0), color);
}

void l2dcat_ui_shell_draw(struct nk_context *context, float width,
    float height, bool dark) {
    L2DCatUIPalette p = l2dcat_ui_palette(dark);
    struct nk_command_buffer *canvas = nk_window_get_canvas(context);
    struct nk_color shadow = rgb(dark ? 0x0D0E11 : 0xD6DBE5);
    struct nk_rect shade = nk_rect(11, 15, width - 22, height - 24);
    struct nk_rect surface = nk_rect(10, 10, width - 20, height - 20);
    nk_fill_rect(canvas, shade, 24, shadow);
    nk_fill_rect(canvas, surface, 24, p.surface);
    nk_stroke_rect(canvas, surface, 24, 1, dark ? p.surface : p.border);
}

static void signature(struct nk_command_buffer *canvas, struct nk_rect bounds,
    float title_width, L2DCatUIPalette p) {
    float width = NK_CLAMP(86.0f, title_width + 16.0f, 188.0f);
    float x = bounds.x + 24.0f, y = bounds.y + 52.0f;
    bool dark = p.surface.r < 128;
    struct nk_color leading = blend(p.accent, rgb(0xA8ECFF), dark ? 38 : 58);
    struct nk_color glow = blend(p.surface, leading, dark ? 24 : 34);
    nk_stroke_curve(canvas, x + 3, y - 3, x + width * .12f, y - 2,
        x + width * .20f, y - 2, x + width * .29f, y - 3, 2, glow);
    nk_stroke_curve(canvas, x, y, x + width * .22f, y + 4,
        x + width * .58f, y + 3, x + width * .66f, y, 7, glow);
    nk_stroke_curve(canvas, x + width * .66f, y, x + width * .74f, y - 3,
        x + width * .52f, y + 5, x + width * .60f, y + 7, 7, glow);
    nk_stroke_curve(canvas, x + width * .60f, y + 7,
        x + width * .68f, y + 9, x + width * .88f, y + 1,
        x + width, y - 5, 7, glow);
    nk_stroke_curve(canvas, x, y, x + width * .22f, y + 4,
        x + width * .58f, y + 3, x + width * .66f, y, 4, leading);
    nk_stroke_curve(canvas, x + width * .66f, y, x + width * .74f, y - 3,
        x + width * .52f, y + 5, x + width * .60f, y + 7, 4, p.accent);
    nk_stroke_curve(canvas, x + width * .60f, y + 7,
        x + width * .68f, y + 9, x + width * .88f, y + 1,
        x + width, y - 5, 4, p.accent);
}

static struct nk_rect close_rect(struct nk_rect bounds) {
    return nk_rect(bounds.x + bounds.w - 56, bounds.y + 16, 36, 36);
}

bool l2dcat_ui_header(struct nk_context *context, const char *title,
    const struct nk_user_font *font, bool interactive, bool dark) {
    struct nk_rect bounds;
    nk_layout_row_dynamic(context, L2DCAT_UI_HEADER_HEIGHT, 1);
    if (nk_widget(&bounds, context) == NK_WIDGET_INVALID) return false;
    L2DCatUIPalette p = l2dcat_ui_palette(dark);
    struct nk_command_buffer *canvas = nk_window_get_canvas(context);
    if (!font) font = context->style.font;
    float title_width = text_width(font, title);
    struct nk_rect text = nk_rect(bounds.x + 28,
        bounds.y + (48.0f - font->height) * .5f,
        NK_MIN(title_width + 1, bounds.w - 92), font->height);
    struct nk_rect title_hit = nk_rect(bounds.x + 20, bounds.y + 12,
        NK_MIN(title_width + 205, bounds.w - 92), 56);
    bool title_hover = interactive &&
        nk_input_is_mouse_hovering_rect(&context->input, title_hit);
    nk_draw_text(canvas, text, title, nk_strlen(title), font,
        nk_rgba(0, 0, 0, 0), title_hover ? rgb(0xF77DAA) : p.accent);
    signature(canvas, bounds, title_width, p);
    struct nk_rect close = close_rect(bounds);
    bool hover = interactive &&
        nk_input_is_mouse_hovering_rect(&context->input, close);
    if (hover) {
        nk_fill_circle(canvas, close, p.field);
        l2dcat_ui_cursor_hover_rect(context, close, L2DCAT_UI_CURSOR_POINTER);
    }
    struct nk_color icon = hover ? p.accent : p.muted;
    nk_stroke_line(canvas, close.x + 11, close.y + 11,
        close.x + 25, close.y + 25, 2, icon);
    nk_stroke_line(canvas, close.x + 25, close.y + 11,
        close.x + 11, close.y + 25, 2, icon);
    return interactive && nk_input_is_mouse_click_in_rect(&context->input,
        NK_BUTTON_LEFT, close);
}

void l2dcat_ui_tabs(struct nk_context *context, const char *const *labels,
    int count, int *active, bool interactive, bool dark) {
    struct nk_rect bounds;
    nk_layout_row_dynamic(context, L2DCAT_UI_TABS_HEIGHT, 1);
    if (nk_widget(&bounds, context) == NK_WIDGET_INVALID || count < 1) return;
    L2DCatUIPalette p = l2dcat_ui_palette(dark);
    struct nk_command_buffer *canvas = nk_window_get_canvas(context);
    const struct nk_user_font *font = l2dcat_ui_label_font(context);
    float widths[8] = {0}, gap = 4.0f, total = gap * (count - 1);
    for (int i = 0; i < count && i < 8; ++i) {
        widths[i] = NK_MAX(82.0f, text_width(font, labels[i]) + 34.0f);
        total += widths[i];
    }
    float available = bounds.w - 48.0f;
    if (total > available) {
        total = available;
        for (int i = 0; i < count && i < 8; ++i)
            widths[i] = (available - gap * (count - 1)) / count;
    }
    float x = bounds.x + (bounds.w - total) * .5f;
    struct nk_rect rail = nk_rect(x - 4, bounds.y + 7, total + 8, 42);
    nk_fill_rect(canvas, rail, 13, p.field);
    for (int i = 0; i < count; ++i) {
        struct nk_rect tab = nk_rect(x, bounds.y + 10, widths[i], 36);
        bool hover = interactive &&
            nk_input_is_mouse_hovering_rect(&context->input, tab);
        bool selected = *active == i;
        if (selected || hover) nk_fill_rect(canvas, tab, 10,
            selected ? p.selection : p.hover);
        if (selected) nk_fill_rect(canvas,
            nk_rect(tab.x + 16, tab.y + tab.h - 3, tab.w - 32, 3), 2, p.accent);
        struct nk_color text_color = selected ? p.accent :
            (hover ? p.text : p.muted);
        draw_centered(canvas, tab, labels[i], font, text_color);
        if (hover) l2dcat_ui_cursor_hover_rect(context, tab,
            L2DCAT_UI_CURSOR_POINTER);
        if (interactive && nk_input_is_mouse_click_in_rect(&context->input,
            NK_BUTTON_LEFT, tab)) *active = i;
        x += widths[i] + gap;
    }
}

bool l2dcat_ui_close_hit(float x, float y, float width) {
    return x >= width - 72.0f && x <= width - 22.0f &&
        y >= 20.0f && y <= 68.0f;
}

bool l2dcat_ui_title_drag_hit(float x, float y, float width) {
    return x >= L2DCAT_UI_MARGIN && x <= width - L2DCAT_UI_MARGIN &&
        y >= L2DCAT_UI_MARGIN && y <= L2DCAT_UI_HEADER_HEIGHT &&
        !l2dcat_ui_close_hit(x, y, width);
}
