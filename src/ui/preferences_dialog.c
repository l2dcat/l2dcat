#include "preferences_internal.h"
#include "ui_backend.h"
#include "l2dcat/i18n.h"

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

static struct nk_color color(bool dark, int light, int night) {
    int value = dark ? night : light;
    return nk_rgb((value >> 16) & 255, (value >> 8) & 255, value & 255);
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
    bool dark = context->style.window.background.r < 128;
    struct nk_style_button style = context->style.button;
    struct nk_color normal = danger ? color(dark, 0xD9363E, 0xD84A4F) :
        color(dark, 0xFFFFFF, 0x1B1F29);
    struct nk_color hover = danger ? color(dark, 0xFF4D4F, 0xE85C61) :
        color(dark, 0xEAF3FF, 0x26354B);
    style.normal = nk_style_item_color(normal);
    style.hover = nk_style_item_color(hover);
    style.active = nk_style_item_color(hover);
    style.border_color = danger ? normal : color(dark, 0xB7C4D8, 0x43516A);
    style.text_normal = danger ? nk_rgb(255, 255, 255) :
        color(dark, 0x344054, 0xD8DEEA);
    style.text_hover = danger ? nk_rgb(255, 255, 255) :
        color(dark, 0x1677FF, 0x69A9FF);
    style.text_active = style.text_hover;
    l2dcat_ui_cursor_hover_widget(context, L2DCAT_UI_CURSOR_POINTER);
    return nk_button_label_styled(context, &style, label) != 0;
}

void l2dcat_preferences_remove_dialog_draw(L2DCatApp *app,
    struct nk_context *context) {
    if (!l2dcat_preferences_remove_dialog_active(app)) return;
    l2dcat_ui_cursor_reset(context);
    struct nk_rect region = nk_window_get_content_region(context);
    float width = NK_MIN(420.0f, region.w - 40.0f);
    float height = 174.0f;
    struct nk_rect bounds = nk_rect((region.w - width) * .5f,
        (region.h - height) * .5f, width, height);
    if (!nk_popup_begin(context, NK_POPUP_STATIC, "remove-model-confirm",
        NK_WINDOW_NO_SCROLLBAR, bounds)) {
        l2dcat_preferences_remove_dialog_clear(app);
        return;
    }
    nk_layout_row_dynamic(context, 30, 1);
    nk_label(context, tr(app, "native.delete", "Delete"), NK_TEXT_LEFT);
    nk_layout_row_dynamic(context, 48, 1);
    nk_label_wrap(context, tr(app, "pages.preference.model.hints.deleteModel",
        "Delete this custom model?"));
    nk_layout_row_dynamic(context, 36, 2);
    if (dialog_button(context, tr(app, "native.cancel", "Cancel"), false)) {
        l2dcat_preferences_remove_dialog_clear(app);
        nk_popup_close(context);
    } else if (dialog_button(context, tr(app, "native.delete", "Delete"), true)) {
        L2DCatError error = {0};
        if (l2dcat_app_remove_model(app, remove_dialog.model_id, &error) != L2DCAT_OK)
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, L2DCAT_NAME,
                error.message, app->window);
        else l2dcat_preferences_remove_dialog_clear(app);
        nk_popup_close(context);
    }
    nk_popup_end(context);
}
