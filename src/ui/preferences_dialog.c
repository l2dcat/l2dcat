#include "preferences_internal.h"
#include "preferences_notice.h"
#include "ui_backend.h"
#include "ui_catime.h"
#include "l2dcat/i18n.h"
#include "l2dcat/preferences.h"

#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

typedef struct RemoveDialog {
    L2DCatApp *app;
    char model_id[L2DCAT_ID_CAP];
} RemoveDialog;

static RemoveDialog remove_dialog;

static const char *tr(L2DCatApp *app, const char *key, const char *fallback) {
    return l2dcat_i18n_get(app->i18n, key, fallback);
}

bool l2dcat_preferences_remove_dialog_active(const L2DCatApp *app) {
    return app && remove_dialog.app == app && remove_dialog.model_id[0];
}

void l2dcat_preferences_remove_dialog_open(L2DCatApp *app, const char *id) {
    if (!app || !id || !id[0]) return;
    remove_dialog.app = app;
    snprintf(remove_dialog.model_id, sizeof(remove_dialog.model_id), "%s", id);
}

void l2dcat_preferences_remove_dialog_clear(const L2DCatApp *app) {
    if (!app || remove_dialog.app == app) memset(&remove_dialog, 0, sizeof(remove_dialog));
}

static bool dialog_button(struct nk_context *context, const char *label,
    bool danger) {
    bool dark = l2dcat_ui_dark(context);
    L2DCatUIPalette palette = l2dcat_ui_palette(dark);
    struct nk_style_button style = context->style.button;
    struct nk_color normal = danger ? palette.danger_background : palette.surface;
    struct nk_color hover = danger ? palette.danger : palette.selection;
    style.normal = nk_style_item_color(normal);
    style.hover = nk_style_item_color(hover);
    style.active = nk_style_item_color(hover);
    style.border_color = danger ? palette.danger : palette.border;
    style.text_normal = danger ? palette.danger : palette.text;
    style.text_hover = danger ? (dark ? palette.background : nk_rgb(255, 255, 255)) :
        palette.accent;
    style.text_active = style.text_hover;
    l2dcat_ui_cursor_hover_widget(context, L2DCAT_UI_CURSOR_POINTER);
    return nk_button_label_styled(context, &style, label) != 0;
}

static float label_width(struct nk_context *context, const char *label) {
    return context->style.font->width(context->style.font->userdata,
        context->style.font->height, label, nk_strlen(label));
}

static bool dialog_header(struct nk_context *context, const char *title,
    L2DCatUIPalette palette) {
    struct nk_rect bounds;
    nk_layout_row_dynamic(context, 36, 1);
    if (nk_widget(&bounds, context) == NK_WIDGET_INVALID) return false;
    struct nk_command_buffer *canvas = nk_window_get_canvas(context);
    float height = context->style.font->height;
    struct nk_rect text = nk_rect(bounds.x, bounds.y + (bounds.h - height) * .5f,
        bounds.w - 42, height);
    nk_draw_text(canvas, text, title, nk_strlen(title), context->style.font,
        nk_rgba(0, 0, 0, 0), palette.accent);
    struct nk_rect close = nk_rect(bounds.x + bounds.w - 32, bounds.y + 2, 30, 30);
    bool hover = nk_input_is_mouse_hovering_rect(&context->input, close);
    if (hover) {
        nk_fill_rect(canvas, close, 8, palette.field);
        l2dcat_ui_cursor_hover_rect(context, close, L2DCAT_UI_CURSOR_POINTER);
    }
    struct nk_color color = hover ? palette.accent : palette.muted;
    nk_stroke_line(canvas, close.x + 9, close.y + 9,
        close.x + 21, close.y + 21, 2, color);
    nk_stroke_line(canvas, close.x + 21, close.y + 9,
        close.x + 9, close.y + 21, 2, color);
    nk_stroke_line(canvas, bounds.x, bounds.y + bounds.h - 1,
        bounds.x + bounds.w, bounds.y + bounds.h - 1, 1, palette.border);
    return nk_input_is_mouse_click_in_rect(&context->input,
        NK_BUTTON_LEFT, close);
}

static void dialog_style_apply(struct nk_context *context,
    L2DCatUIPalette palette) {
    context->style.window.fixed_background = nk_style_item_color(palette.surface);
    context->style.window.background = palette.surface;
    context->style.window.border_color = palette.border;
    context->style.window.padding = nk_vec2(20, 16);
    context->style.window.spacing = nk_vec2(10, 10);
    context->style.window.border = 1;
    context->style.window.rounding = 16;
}

static void dialog_actions(L2DCatApp *app, struct nk_context *context,
    bool *close_requested) {
    const char *cancel = tr(app, "native.cancel", "Cancel");
    const char *remove = tr(app, "native.delete", "Delete");
    float cancel_width = NK_MAX(88.0f, label_width(context, cancel) + 30.0f);
    float remove_width = NK_MAX(88.0f, label_width(context, remove) + 30.0f);
    float available = nk_window_get_content_region(context).w;
    nk_layout_row_begin(context, NK_STATIC, 36, 3);
    nk_layout_row_push(context, NK_MAX(0.0f,
        available - cancel_width - remove_width - 10.0f));
    nk_spacing(context, 1);
    nk_layout_row_push(context, cancel_width);
    if (dialog_button(context, cancel, false)) *close_requested = true;
    nk_layout_row_push(context, remove_width);
    if (dialog_button(context, remove, true)) {
        L2DCatError error = {0};
        if (l2dcat_app_remove_model(app, remove_dialog.model_id,
            &error) != L2DCAT_OK)
            l2dcat_preferences_notice_show(app, error.message, true);
        *close_requested = true;
    }
    nk_layout_row_end(context);
}

void l2dcat_preferences_remove_dialog_draw(L2DCatApp *app,
    struct nk_context *context) {
    if (!l2dcat_preferences_remove_dialog_active(app)) return;
    l2dcat_ui_cursor_reset(context);
    struct nk_rect region = nk_window_get_content_region(context);
    float width = NK_MIN(420.0f, region.w - 48.0f);
    float height = 202.0f;
    float local_x = (region.w - width) * .5f;
    float local_y = (region.h - height) * .5f;
    struct nk_rect bounds = nk_rect(local_x, local_y, width, height);
    L2DCatUIPalette palette = l2dcat_ui_palette(l2dcat_ui_dark(context));
    struct nk_command_buffer *canvas = nk_window_get_canvas(context);
    nk_fill_rect(canvas, region, 0, nk_rgba(0, 0, 0,
        l2dcat_ui_dark(context) ? 96 : 48));
    nk_fill_rect(canvas, nk_rect(region.x + local_x + 2,
        region.y + local_y + 5, width, height), 18,
        nk_rgba(13, 14, 17, l2dcat_ui_dark(context) ? 150 : 55));
    struct nk_style_window saved = context->style.window;
    dialog_style_apply(context, palette);
    if (!nk_popup_begin(context, NK_POPUP_STATIC, "remove-model-confirm",
        NK_WINDOW_NO_SCROLLBAR, bounds)) {
        context->style.window = saved;
        l2dcat_preferences_remove_dialog_clear(app);
        return;
    }
    bool close_requested = dialog_header(context,
        tr(app, "native.delete", "Delete"), palette);
    nk_layout_row_dynamic(context, 58, 1);
    nk_label_colored_wrap(context, tr(app,
        "pages.preference.model.hints.deleteModel",
        "Delete this custom model?"), palette.text);
    dialog_actions(app, context, &close_requested);
    if (close_requested) {
        l2dcat_preferences_remove_dialog_clear(app);
        nk_popup_close(context);
        l2dcat_preferences_invalidate(app->preferences);
    }
    nk_popup_end(context);
    context->style.window = saved;
}
