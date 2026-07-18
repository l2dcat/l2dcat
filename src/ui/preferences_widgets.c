#include "preferences_widgets.h"

typedef struct CardStyle {
    struct nk_style_item background;
    struct nk_color window_color;
    struct nk_color border_color;
    struct nk_vec2 padding;
    float border;
} CardStyle;

static bool dark(const struct nk_context *context) {
    return context->style.window.background.r < 128;
}

static struct nk_color color(bool night, int light, int dark_value) {
    int value = night ? dark_value : light;
    return nk_rgb((value >> 16) & 255, (value >> 8) & 255, value & 255);
}

static bool card_begin(struct nk_context *context, const char *id,
    float height, CardStyle *saved) {
    bool night = dark(context);
    saved->background = context->style.window.fixed_background;
    saved->window_color = context->style.window.background;
    saved->border_color = context->style.window.group_border_color;
    saved->padding = context->style.window.group_padding;
    saved->border = context->style.window.group_border;
    struct nk_color raised = color(night, 0xFFFFFF, 0x1B1F29);
    context->style.window.fixed_background = nk_style_item_color(raised);
    context->style.window.background = raised;
    context->style.window.group_border_color = color(night, 0xE3E8F0, 0x30394A);
    context->style.window.group_padding = nk_vec2(16, 11);
    context->style.window.group_border = 1;
    nk_layout_row_dynamic(context, height, 1);
    bool visible = nk_group_begin(context, id,
        NK_WINDOW_BORDER | NK_WINDOW_NO_SCROLLBAR) != 0;
    if (!visible) {
        context->style.window.fixed_background = saved->background;
        context->style.window.background = saved->window_color;
        context->style.window.group_border_color = saved->border_color;
        context->style.window.group_padding = saved->padding;
        context->style.window.group_border = saved->border;
    }
    return visible;
}

static void card_end(struct nk_context *context, const CardStyle *saved) {
    nk_group_end(context);
    context->style.window.fixed_background = saved->background;
    context->style.window.background = saved->window_color;
    context->style.window.group_border_color = saved->border_color;
    context->style.window.group_padding = saved->padding;
    context->style.window.group_border = saved->border;
}

static void description(struct nk_context *context, const char *text) {
    if (!text || !text[0]) return;
    nk_layout_row_dynamic(context, 22, 1);
    nk_label_colored(context, text, NK_TEXT_LEFT,
        color(dark(context), 0x8C8C8C, 0x8C96A8));
}

static bool toggle(struct nk_context *context, bool *value) {
    struct nk_rect cell;
    if (nk_widget(&cell, context) == NK_WIDGET_INVALID) return false;
    struct nk_rect track = nk_rect(cell.x + cell.w - 44,
        cell.y + (cell.h - 24) * .5f, 44, 24);
    bool hover = nk_input_is_mouse_hovering_rect(&context->input, track);
    bool changed = nk_input_is_mouse_click_in_rect(&context->input,
        NK_BUTTON_LEFT, track);
    if (changed) *value = !*value;
    bool night = dark(context);
    struct nk_color background = *value ? color(night, 0x1677FF, 0x1668DC) :
        color(night, hover ? 0xBFBFBF : 0xD9D9D9, hover ? 0x596273 : 0x454D5C);
    struct nk_command_buffer *canvas = nk_window_get_canvas(context);
    nk_fill_rect(canvas, track, 12, background);
    float knob_x = *value ? track.x + 23 : track.x + 3;
    nk_fill_circle(canvas, nk_rect(knob_x, track.y + 3, 18, 18), nk_rgb(255, 255, 255));
    return changed;
}

void l2dcat_pref_section(struct nk_context *context, const char *title) {
    nk_layout_row_dynamic(context, 34, 1);
    nk_label(context, title, NK_TEXT_LEFT);
}

bool l2dcat_pref_toggle(struct nk_context *context, const char *id,
    const char *title, const char *detail, bool *value) {
    CardStyle saved;
    if (!card_begin(context, id, detail && detail[0] ? 78.0f : 58.0f, &saved)) return false;
    nk_layout_row_begin(context, NK_DYNAMIC, 30, 2);
    nk_layout_row_push(context, .76f); nk_label(context, title, NK_TEXT_LEFT);
    nk_layout_row_push(context, .24f); bool changed = toggle(context, value);
    nk_layout_row_end(context);
    description(context, detail);
    card_end(context, &saved);
    return changed;
}

static void card_title(struct nk_context *context, const char *title) {
    nk_layout_row_push(context, .62f);
    nk_label(context, title, NK_TEXT_LEFT);
    nk_layout_row_push(context, .38f);
}

bool l2dcat_pref_float(struct nk_context *context, const char *id,
    const char *title, const char *detail, float minimum, float *value,
    float maximum, float step) {
    CardStyle saved;
    if (!card_begin(context, id, 78, &saved)) return false;
    nk_layout_row_begin(context, NK_DYNAMIC, 30, 2); card_title(context, title);
    bool changed = nk_property_float(context, "#", minimum, value, maximum,
        step, step * .1f) != 0;
    nk_layout_row_end(context); description(context, detail); card_end(context, &saved);
    return changed;
}

bool l2dcat_pref_int(struct nk_context *context, const char *id,
    const char *title, const char *detail, int minimum, int *value,
    int maximum, int step) {
    CardStyle saved;
    if (!card_begin(context, id, 78, &saved)) return false;
    nk_layout_row_begin(context, NK_DYNAMIC, 30, 2); card_title(context, title);
    bool changed = nk_property_int(context, "#", minimum, value, maximum,
        step, 1.0f) != 0;
    nk_layout_row_end(context); description(context, detail); card_end(context, &saved);
    return changed;
}

bool l2dcat_pref_slider(struct nk_context *context, const char *id,
    const char *title, const char *detail, float minimum, float *value,
    float maximum, float step) {
    CardStyle saved;
    if (!card_begin(context, id, 78, &saved)) return false;
    float before = *value;
    nk_layout_row_begin(context, NK_DYNAMIC, 30, 2); card_title(context, title);
    nk_slider_float(context, minimum, value, maximum, step);
    nk_layout_row_end(context); description(context, detail); card_end(context, &saved);
    return before != *value;
}

int l2dcat_pref_combo(struct nk_context *context, const char *id,
    const char *title, const char *detail, const char *const *items,
    int count, int selected) {
    CardStyle saved;
    if (!card_begin(context, id, 78, &saved)) return selected;
    nk_layout_row_begin(context, NK_DYNAMIC, 30, 2); card_title(context, title);
    selected = nk_combo(context, items, count, selected, 32, nk_vec2(280, 220));
    nk_layout_row_end(context); description(context, detail); card_end(context, &saved);
    return selected;
}

void l2dcat_pref_edit(struct nk_context *context, const char *id,
    const char *title, const char *detail, char *value, int capacity) {
    CardStyle saved;
    if (!card_begin(context, id, 78, &saved)) return;
    nk_layout_row_begin(context, NK_DYNAMIC, 30, 2); card_title(context, title);
    nk_edit_string_zero_terminated(context, NK_EDIT_FIELD, value, capacity, nk_filter_default);
    nk_layout_row_end(context); description(context, detail); card_end(context, &saved);
}

bool l2dcat_pref_button(struct nk_context *context, const char *id,
    const char *title, const char *detail, const char *button) {
    CardStyle saved;
    if (!card_begin(context, id, 78, &saved)) return false;
    nk_layout_row_begin(context, NK_DYNAMIC, 30, 2); card_title(context, title);
    bool clicked = nk_button_label(context, button) != 0;
    nk_layout_row_end(context); description(context, detail); card_end(context, &saved);
    return clicked;
}

void l2dcat_pref_status(struct nk_context *context, const char *id,
    const char *title, const char *detail) {
    CardStyle saved;
    if (!card_begin(context, id, 64, &saved)) return;
    nk_layout_row_dynamic(context, 24, 1); nk_label(context, title, NK_TEXT_LEFT);
    description(context, detail); card_end(context, &saved);
}
