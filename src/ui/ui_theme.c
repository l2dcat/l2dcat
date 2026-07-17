#include "ui_theme.h"

static struct nk_color color(bool dark, int light, int night) {
    int value = dark ? night : light;
    return nk_rgba((value >> 16) & 255, (value >> 8) & 255, value & 255, 255);
}

static struct nk_style_item item(struct nk_color value) {
    return nk_style_item_color(value);
}

static void apply_geometry(struct nk_style *style) {
    style->window.padding = nk_vec2(14, 12);
    style->window.group_padding = nk_vec2(16, 14);
    style->window.spacing = nk_vec2(10, 9);
    style->window.scrollbar_size = nk_vec2(10, 10);
    style->window.border = 0;
    style->window.group_border = 1;
    style->window.rounding = 14;
    style->button.border = 1;
    style->button.rounding = 10;
    style->button.padding = nk_vec2(12, 7);
    style->checkbox.padding = nk_vec2(3, 3);
    style->checkbox.spacing = 10;
    style->checkbox.border = 1;
    style->option = style->checkbox;
    style->edit.border = 1;
    style->edit.rounding = 8;
    style->edit.padding = nk_vec2(10, 7);
    style->property.border = 1;
    style->property.rounding = 8;
    style->property.padding = nk_vec2(6, 5);
    style->property.edit.rounding = 6;
    style->property.inc_button.rounding = 6;
    style->property.dec_button.rounding = 6;
    style->combo.border = 1;
    style->combo.rounding = 8;
    style->combo.content_padding = nk_vec2(10, 7);
    style->combo.button.rounding = 6;
    style->selectable.rounding = 8;
    style->scrollv.rounding = 5;
    style->scrollv.rounding_cursor = 5;
    style->scrollh = style->scrollv;
}

void bongo_ui_apply_theme(struct nk_context *context, bool dark) {
    struct nk_color table[NK_COLOR_COUNT];
    struct nk_color text = color(dark, 0x202431, 0xECEEF5);
    struct nk_color muted = color(dark, 0x667085, 0xA7AFC0);
    struct nk_color base = color(dark, 0xF2F5F9, 0x0D1118);
    struct nk_color surface = color(dark, 0xFFFFFF, 0x1B1F29);
    struct nk_color raised = color(dark, 0xEEF1F7, 0x252B38);
    struct nk_color border = color(dark, 0xE1E6EF, 0x30394A);
    struct nk_color accent = color(dark, 0x1677FF, 0x1668DC);
    struct nk_color accent_hover = color(dark, 0x4096FF, 0x3C89E8);
    table[NK_COLOR_TEXT] = text;
    table[NK_COLOR_WINDOW] = base;
    table[NK_COLOR_HEADER] = surface;
    table[NK_COLOR_BORDER] = border;
    table[NK_COLOR_BUTTON] = accent;
    table[NK_COLOR_BUTTON_HOVER] = accent_hover;
    table[NK_COLOR_BUTTON_ACTIVE] = color(dark, 0x0958D9, 0x1554AD);
    table[NK_COLOR_TOGGLE] = raised;
    table[NK_COLOR_TOGGLE_HOVER] = border;
    table[NK_COLOR_TOGGLE_CURSOR] = accent;
    table[NK_COLOR_SELECT] = raised;
    table[NK_COLOR_SELECT_ACTIVE] = accent;
    table[NK_COLOR_SLIDER] = raised;
    table[NK_COLOR_SLIDER_CURSOR] = accent;
    table[NK_COLOR_SLIDER_CURSOR_HOVER] = accent_hover;
    table[NK_COLOR_SLIDER_CURSOR_ACTIVE] = accent_hover;
    table[NK_COLOR_PROPERTY] = surface;
    table[NK_COLOR_EDIT] = surface;
    table[NK_COLOR_EDIT_CURSOR] = text;
    table[NK_COLOR_COMBO] = surface;
    table[NK_COLOR_CHART] = surface;
    table[NK_COLOR_CHART_COLOR] = accent;
    table[NK_COLOR_CHART_COLOR_HIGHLIGHT] = accent_hover;
    table[NK_COLOR_SCROLLBAR] = base;
    table[NK_COLOR_SCROLLBAR_CURSOR] = border;
    table[NK_COLOR_SCROLLBAR_CURSOR_HOVER] = muted;
    table[NK_COLOR_SCROLLBAR_CURSOR_ACTIVE] = accent;
    table[NK_COLOR_TAB_HEADER] = surface;
    table[NK_COLOR_KNOB] = raised;
    table[NK_COLOR_KNOB_CURSOR] = accent;
    table[NK_COLOR_KNOB_CURSOR_HOVER] = accent_hover;
    table[NK_COLOR_KNOB_CURSOR_ACTIVE] = accent_hover;
    nk_style_from_table(context, table);
    apply_geometry(&context->style);
    context->style.button.text_normal = nk_rgb(255, 255, 255);
    context->style.button.text_hover = nk_rgb(255, 255, 255);
    context->style.button.text_active = nk_rgb(255, 255, 255);
    context->style.window.fixed_background = item(base);
    context->style.window.group_border_color = border;
    context->style.text.color = text;
}

bool bongo_ui_nav_button(struct nk_context *context, const char *label,
    bool active, bool dark) {
    struct nk_style_button style = context->style.button;
    struct nk_color base = color(dark, 0xFFFFFF, 0x1B1F29);
    struct nk_color hover = color(dark, 0xEEF1F7, 0x252B38);
    struct nk_color text = color(dark, 0x4B5565, 0xB9C0CF);
    struct nk_color accent = color(dark, 0x1677FF, 0x1668DC);
    style.normal = item(active ? accent : base);
    style.hover = item(active ? accent : hover);
    style.active = item(accent);
    style.border_color = active ? accent : color(dark, 0xD9DEE9, 0x343C4C);
    style.text_normal = active ? nk_rgb(255, 255, 255) : text;
    style.text_hover = active ? nk_rgb(255, 255, 255) : accent;
    style.text_active = nk_rgb(255, 255, 255);
    return nk_button_label_styled(context, &style, label) != 0;
}
