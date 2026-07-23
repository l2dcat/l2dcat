#include "preferences_widgets.h"
#include "preferences_controls.h"
#include "ui_backend.h"
#include "ui_catime.h"

typedef struct FormStyle {
    struct nk_style_item background;
    struct nk_color window_color;
    struct nk_color border_color;
    struct nk_vec2 padding;
    struct nk_vec2 spacing;
    float border;
} FormStyle;

static int detail_lines(const struct nk_context *context, const char *text) {
    if (!text || !text[0]) return 0;
    const struct nk_user_font *font = bongo_cat_neo_ui_caption_font(context);
    float width = nk_window_get_content_region(context).w - 310.0f;
    if (width < 220.0f) width = 220.0f;
    float measured = font->width(font->userdata, font->height,
        text, nk_strlen(text));
    int lines = (int)(measured / width) + 1;
    return NK_CLAMP(1, lines, 3);
}

static void restore_style(struct nk_context *context, const FormStyle *saved) {
    context->style.window.fixed_background = saved->background;
    context->style.window.background = saved->window_color;
    context->style.window.group_border_color = saved->border_color;
    context->style.window.group_padding = saved->padding;
    context->style.window.spacing = saved->spacing;
    context->style.window.group_border = saved->border;
}

static bool form_begin(struct nk_context *context, const char *id,
    int lines, FormStyle *saved) {
    saved->background = context->style.window.fixed_background;
    saved->window_color = context->style.window.background;
    saved->border_color = context->style.window.group_border_color;
    saved->padding = context->style.window.group_padding;
    saved->spacing = context->style.window.spacing;
    saved->border = context->style.window.group_border;
    struct nk_color clear = nk_rgba(0, 0, 0, 0);
    context->style.window.fixed_background = nk_style_item_color(clear);
    context->style.window.background = clear;
    context->style.window.group_border_color = clear;
    context->style.window.group_padding = nk_vec2(18, 12);
    context->style.window.spacing = nk_vec2(8, 5);
    context->style.window.group_border = 0;
    nk_layout_row_dynamic(context, lines ? 80.0f + lines * 22.0f : 76.0f, 1);
    if (!nk_group_begin(context, id, NK_WINDOW_NO_SCROLLBAR)) {
        restore_style(context, saved);
        return false;
    }
    struct nk_rect bounds = nk_window_get_bounds(context);
    BongoCatNeoUIPalette p = bongo_cat_neo_ui_palette(bongo_cat_neo_ui_dark(context));
    bool hover = nk_input_is_mouse_hovering_rect(&context->input, bounds);
    struct nk_command_buffer *canvas = nk_window_get_canvas(context);
    nk_fill_rect(canvas, bounds, 14, hover ? p.hover : p.surface);
    nk_stroke_rect(canvas, bounds, 14, 1, hover ? p.accent : p.border);
    return true;
}

static void form_end(struct nk_context *context, const FormStyle *saved) {
    nk_group_end(context);
    restore_style(context, saved);
}

static float left_width(struct nk_context *context) {
    return NK_MAX(220.0f, nk_window_get_content_region(context).w - 278.0f);
}

static void form_title(struct nk_context *context, const char *title) {
    float left = left_width(context);
    nk_layout_row_begin(context, NK_STATIC, 48, 2);
    nk_layout_row_push(context, left);
    nk_style_push_font(context, bongo_cat_neo_ui_label_font(context));
    nk_label(context, title, NK_TEXT_LEFT);
    nk_style_pop_font(context);
    nk_layout_row_push(context, NK_MAX(190.0f,
        nk_window_get_content_region(context).w - left - 8.0f));
}

static void description(struct nk_context *context, const char *text,
    int lines) {
    if (!lines) return;
    BongoCatNeoUIPalette p = bongo_cat_neo_ui_palette(bongo_cat_neo_ui_dark(context));
    float left = left_width(context);
    nk_layout_row_begin(context, NK_STATIC, 22.0f * lines, 2);
    nk_layout_row_push(context, left);
    nk_style_push_font(context, bongo_cat_neo_ui_caption_font(context));
    nk_label_colored_wrap(context, text, p.muted);
    nk_style_pop_font(context);
    nk_layout_row_push(context, NK_MAX(1.0f,
        nk_window_get_content_region(context).w - left - 8.0f));
    nk_spacing(context, 1);
    nk_layout_row_end(context);
}

static bool toggle(struct nk_context *context, bool *value) {
    struct nk_rect cell;
    if (nk_widget(&cell, context) == NK_WIDGET_INVALID) return false;
    struct nk_rect track = nk_rect(cell.x + cell.w - 50,
        cell.y + (cell.h - 28) * .5f, 48, 28);
    bool hover = nk_input_is_mouse_hovering_rect(&context->input, track);
    bool changed = hover && nk_input_is_mouse_click_in_rect(&context->input,
        NK_BUTTON_LEFT, track);
    if (changed) *value = !*value;
    BongoCatNeoUIPalette p = bongo_cat_neo_ui_palette(bongo_cat_neo_ui_dark(context));
    struct nk_command_buffer *canvas = nk_window_get_canvas(context);
    nk_fill_rect(canvas, track, 14, *value ? p.accent : p.field);
    nk_stroke_rect(canvas, track, 14, hover ? 2.0f : 1.0f,
        hover ? p.accent : (*value ? p.accent : p.border));
    float knob_x = *value ? track.x + 24 : track.x + 4;
    nk_fill_circle(canvas, nk_rect(knob_x, track.y + 4, 20, 20),
        nk_rgb(255, 255, 255));
    if (hover) bongo_cat_neo_ui_cursor_hover_rect(context, track,
        BONGO_CAT_NEO_UI_CURSOR_POINTER);
    return changed;
}

static bool secondary_button(struct nk_context *context, const char *label) {
    BongoCatNeoUIPalette p = bongo_cat_neo_ui_palette(bongo_cat_neo_ui_dark(context));
    struct nk_style_button style = context->style.button;
    style.normal = nk_style_item_color(p.field);
    style.hover = nk_style_item_color(p.selection);
    style.active = nk_style_item_color(p.selection);
    style.border_color = p.border;
    style.border = 1;
    style.rounding = 10;
    style.text_normal = p.text;
    style.text_hover = p.accent;
    style.text_active = p.accent;
    struct nk_rect bounds = nk_widget_bounds(context);
    if (nk_input_is_mouse_hovering_rect(&context->input, bounds))
        bongo_cat_neo_ui_cursor_hover_rect(context, bounds, BONGO_CAT_NEO_UI_CURSOR_POINTER);
    return nk_button_label_styled(context, &style, label) != 0;
}

void bongo_cat_neo_pref_section(struct nk_context *context, const char *title) {
    struct nk_rect bounds;
    nk_layout_row_dynamic(context, 54, 1);
    if (nk_widget(&bounds, context) == NK_WIDGET_INVALID) return;
    BongoCatNeoUIPalette p = bongo_cat_neo_ui_palette(bongo_cat_neo_ui_dark(context));
    struct nk_command_buffer *canvas = nk_window_get_canvas(context);
    nk_fill_rect(canvas, nk_rect(bounds.x, bounds.y + 15, 4, 24), 2, p.accent);
    const struct nk_user_font *font = bongo_cat_neo_ui_label_font(context);
    struct nk_rect text = nk_rect(bounds.x + 14,
        bounds.y + (bounds.h - font->height) * .5f, bounds.w - 14, font->height);
    nk_draw_text(canvas, text, title, nk_strlen(title), font,
        nk_rgba(0, 0, 0, 0), p.text);
}

bool bongo_cat_neo_pref_toggle(struct nk_context *context, const char *id,
    const char *title, const char *detail, bool *value) {
    int lines = detail_lines(context, detail); FormStyle saved;
    if (!form_begin(context, id, lines, &saved)) return false;
    form_title(context, title); bool changed = toggle(context, value);
    nk_layout_row_end(context); description(context, detail, lines);
    form_end(context, &saved); return changed;
}

bool bongo_cat_neo_pref_float(struct nk_context *context, const char *id,
    const char *title, const char *detail, float minimum, float *value,
    float maximum, float step) {
    int lines = detail_lines(context, detail); FormStyle saved;
    if (!form_begin(context, id, lines, &saved)) return false;
    form_title(context, title);
    bool changed = bongo_cat_neo_pref_control_float(context, id,
        minimum, value, maximum, step);
    nk_layout_row_end(context); description(context, detail, lines);
    form_end(context, &saved); return changed;
}

bool bongo_cat_neo_pref_int(struct nk_context *context, const char *id,
    const char *title, const char *detail, int minimum, int *value,
    int maximum, int step) {
    int lines = detail_lines(context, detail); FormStyle saved;
    if (!form_begin(context, id, lines, &saved)) return false;
    form_title(context, title);
    bool changed = bongo_cat_neo_pref_control_int(context, id,
        minimum, value, maximum, step);
    nk_layout_row_end(context); description(context, detail, lines);
    form_end(context, &saved); return changed;
}

bool bongo_cat_neo_pref_slider(struct nk_context *context, const char *id,
    const char *title, const char *detail, float minimum, float *value,
    float maximum, float step) {
    int lines = detail_lines(context, detail); FormStyle saved;
    if (!form_begin(context, id, lines, &saved)) return false;
    form_title(context, title);
    bool changed = bongo_cat_neo_pref_control_slider(context, id,
        minimum, value, maximum, step);
    nk_layout_row_end(context); description(context, detail, lines);
    form_end(context, &saved); return changed;
}

int bongo_cat_neo_pref_combo(struct nk_context *context, const char *id,
    const char *title, const char *detail, const char *const *items,
    int count, int selected) {
    int lines = detail_lines(context, detail); FormStyle saved;
    if (!form_begin(context, id, lines, &saved)) return selected;
    form_title(context, title);
    selected = bongo_cat_neo_pref_control_combo(context, items, count, selected);
    nk_layout_row_end(context); description(context, detail, lines);
    form_end(context, &saved); return selected;
}

void bongo_cat_neo_pref_edit(struct nk_context *context, const char *id,
    const char *title, const char *detail, char *value, int capacity) {
    int lines = detail_lines(context, detail); FormStyle saved;
    if (!form_begin(context, id, lines, &saved)) return;
    form_title(context, title);
    struct nk_rect bounds = nk_widget_bounds(context);
    struct nk_style_edit edit = context->style.edit;
    BongoCatNeoUIPalette p = bongo_cat_neo_ui_palette(bongo_cat_neo_ui_dark(context));
    context->style.edit.normal = context->style.edit.hover =
        context->style.edit.active = nk_style_item_color(p.field);
    context->style.edit.border_color = p.border;
    context->style.edit.border = 1;
    context->style.edit.rounding = 10;
    context->style.edit.padding = nk_vec2(13, 8);
    context->style.edit.text_normal = context->style.edit.text_hover =
        context->style.edit.text_active = p.text;
    context->style.edit.cursor_normal = context->style.edit.cursor_hover = p.accent;
    nk_flags state = nk_edit_string_zero_terminated(context, NK_EDIT_FIELD,
        value, capacity, nk_filter_default);
    bool hover = nk_input_is_mouse_hovering_rect(&context->input, bounds);
    if (hover || (state & NK_EDIT_ACTIVE)) nk_stroke_rect(
        nk_window_get_canvas(context), bounds, 10,
        state & NK_EDIT_ACTIVE ? 2.0f : 1.0f, p.accent);
    if (hover) bongo_cat_neo_ui_cursor_hover_rect(context, bounds, BONGO_CAT_NEO_UI_CURSOR_TEXT);
    context->style.edit = edit;
    nk_layout_row_end(context); description(context, detail, lines);
    form_end(context, &saved);
}

bool bongo_cat_neo_pref_button(struct nk_context *context, const char *id,
    const char *title, const char *detail, const char *button) {
    int lines = detail_lines(context, detail); FormStyle saved;
    if (!form_begin(context, id, lines, &saved)) return false;
    form_title(context, title); bool result = secondary_button(context, button);
    nk_layout_row_end(context); description(context, detail, lines);
    form_end(context, &saved); return result;
}

void bongo_cat_neo_pref_status(struct nk_context *context, const char *id,
    const char *title, const char *detail) {
    int lines = detail_lines(context, detail); FormStyle saved;
    if (!form_begin(context, id, lines, &saved)) return;
    BongoCatNeoUIPalette p = bongo_cat_neo_ui_palette(bongo_cat_neo_ui_dark(context));
    nk_layout_row_dynamic(context, 44, 1);
    struct nk_rect row; nk_widget(&row, context);
    nk_fill_circle(nk_window_get_canvas(context),
        nk_rect(row.x, row.y + (row.h - 10) * .5f, 10, 10), p.accent);
    const struct nk_user_font *font = bongo_cat_neo_ui_label_font(context);
    struct nk_rect text = nk_rect(row.x + 20,
        row.y + (row.h - font->height) * .5f, row.w - 20, font->height);
    nk_draw_text(nk_window_get_canvas(context), text, title, nk_strlen(title),
        font, nk_rgba(0, 0, 0, 0), p.text);
    description(context, detail, lines); form_end(context, &saved);
}
