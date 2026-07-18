#include "preferences_internal.h"
#include "preferences_widgets.h"
#include "l2dcat/i18n.h"
#include "l2dcat/preferences.h"

#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

static const char *tr(L2DCatApp *app, const char *key, const char *fallback) {
    return l2dcat_i18n_get(app->i18n, key, fallback);
}

void l2dcat_preferences_import_path(L2DCatApp *app, SDL_Window *window,
    const char *path) {
    if (!app || !path || !path[0]) return;
    L2DCatError error = {0};
    L2DCatResult result = l2dcat_app_import_model(app, path, &error);
    const char *message = result == L2DCAT_OK
        ? tr(app, "pages.preference.model.hints.importSuccess", "Model imported")
        : error.message;
    if (app->smoke) {
        if (result != L2DCAT_OK) app->exit_code = 1;
        return;
    }
    SDL_ShowSimpleMessageBox(result == L2DCAT_OK ? SDL_MESSAGEBOX_INFORMATION :
        SDL_MESSAGEBOX_ERROR, L2DCAT_NAME, message, window);
}

static struct nk_color ui_color(bool dark, int light, int night) {
    int value = dark ? night : light;
    return nk_rgb((value >> 16) & 255, (value >> 8) & 255, value & 255);
}

static bool dark_theme(const struct nk_context *context) {
    return context->style.window.background.r < 128;
}

static void draw_text(struct nk_context *context, struct nk_command_buffer *canvas,
    struct nk_rect bounds, const char *text, struct nk_color color) {
    nk_draw_text(canvas, bounds, text, (int)strlen(text), context->style.font,
        nk_rgba(0, 0, 0, 0), color);
}

static const char *mode_label(L2DCatApp *app, L2DCatModelMode mode) {
    if (mode == L2DCAT_MODE_KEYBOARD)
        return tr(app, "native.modeKeyboard", "Keyboard");
    if (mode == L2DCAT_MODE_GAMEPAD)
        return tr(app, "native.modeGamepad", "Gamepad");
    return tr(app, "native.modeStandard", "Standard");
}

static void draw_model_icon(struct nk_command_buffer *canvas, L2DCatModelMode mode,
    struct nk_rect bounds, struct nk_color color) {
    float cx = bounds.x + bounds.w * .5f, cy = bounds.y + bounds.h * .5f;
    if (mode == L2DCAT_MODE_KEYBOARD) {
        struct nk_rect keyboard = nk_rect(cx - 38, cy - 20, 76, 40);
        nk_stroke_rect(canvas, keyboard, 7, 2, color);
        for (int row = 0; row < 2; ++row)
            for (int col = 0; col < 6; ++col)
                nk_fill_rect(canvas, nk_rect(keyboard.x + 9 + col * 10,
                    keyboard.y + 9 + row * 13, 6, 5), 1, color);
        return;
    }
    if (mode == L2DCAT_MODE_GAMEPAD) {
        nk_stroke_rect(canvas, nk_rect(cx - 42, cy - 23, 84, 46), 18, 2, color);
        nk_stroke_line(canvas, cx - 27, cy - 8, cx - 27, cy + 10, 3, color);
        nk_stroke_line(canvas, cx - 36, cy + 1, cx - 18, cy + 1, 3, color);
        nk_fill_circle(canvas, nk_rect(cx + 17, cy - 10, 8, 8), color);
        nk_fill_circle(canvas, nk_rect(cx + 28, cy + 2, 8, 8), color);
        return;
    }
    nk_stroke_circle(canvas, nk_rect(cx - 31, cy - 29, 62, 58), 2, color);
    nk_stroke_triangle(canvas, cx - 29, cy - 17, cx - 25, cy - 42,
        cx - 8, cy - 27, 2, color);
    nk_stroke_triangle(canvas, cx + 8, cy - 27, cx + 25, cy - 42,
        cx + 29, cy - 17, 2, color);
    nk_fill_circle(canvas, nk_rect(cx - 16, cy - 5, 5, 5), color);
    nk_fill_circle(canvas, nk_rect(cx + 11, cy - 5, 5, 5), color);
}

static L2DCatBehaviorShortcut *behavior_shortcut(L2DCatConfig *config,
    const char *id) {
    for (size_t i = 0; i < config->behavior_shortcut_count; ++i)
        if (strcmp(config->behavior_shortcuts[i].id, id) == 0)
            return &config->behavior_shortcuts[i];
    if (config->behavior_shortcut_count >= L2DCAT_BEHAVIOR_CAP) return NULL;
    L2DCatBehaviorShortcut *value =
        &config->behavior_shortcuts[config->behavior_shortcut_count++];
    snprintf(value->id, sizeof(value->id), "%s", id);
    return value;
}

static bool confirm_remove(L2DCatApp *app, const L2DCatModelEntry *entry) {
    const SDL_MessageBoxButtonData buttons[] = {
        {SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, tr(app, "native.cancel", "Cancel")},
        {SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, tr(app, "native.delete", "Delete")}};
    SDL_MessageBoxData data = {SDL_MESSAGEBOX_WARNING, app->window, L2DCAT_NAME,
        tr(app, "pages.preference.model.hints.deleteModel", "Delete this custom model?"),
        2, buttons, NULL};
    int choice = 0;
    if (!SDL_ShowMessageBox(&data, &choice) || choice != 1) return false;
    L2DCatError error = {0};
    if (l2dcat_app_remove_model(app, entry->id, &error) == L2DCAT_OK) return true;
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, L2DCAT_NAME, error.message, app->window);
    return false;
}

static bool import_card(L2DCatApp *app, struct nk_context *context) {
    struct nk_rect bounds;
    if (nk_widget(&bounds, context) == NK_WIDGET_INVALID) return false;
    bool dark = dark_theme(context);
    bool hover = nk_input_is_mouse_hovering_rect(&context->input, bounds);
    struct nk_command_buffer *canvas = nk_window_get_canvas(context);
    struct nk_color accent = ui_color(dark, 0x1677FF, 0x3C89E8);
    nk_fill_rect(canvas, bounds, 8, ui_color(dark,
        hover ? 0xF0F7FF : 0xFFFFFF, hover ? 0x222A37 : 0x1B1F29));
    nk_stroke_rect(canvas, bounds, 8, 2, accent);
    float cx = bounds.x + bounds.w * .5f;
    nk_stroke_circle(canvas, nk_rect(cx - 20, bounds.y + 35, 40, 40), 2, accent);
    nk_stroke_line(canvas, cx - 9, bounds.y + 55, cx + 9, bounds.y + 55, 3, accent);
    nk_stroke_line(canvas, cx, bounds.y + 46, cx, bounds.y + 64, 3, accent);
    const char *label = tr(app, "pages.preference.model.hints.clickOrDragToImport",
        "Import model directory");
    draw_text(context, canvas, nk_rect(bounds.x + 12, bounds.y + 100,
        bounds.w - 24, 28), label, accent);
    return nk_input_is_mouse_click_in_rect(&context->input, NK_BUTTON_LEFT, bounds);
}

static bool model_card(L2DCatApp *app, struct nk_context *context,
    L2DCatModelEntry *entry) {
    bool selected = strcmp(entry->id, app->config.current_model) == 0;
    struct nk_rect bounds;
    if (nk_widget(&bounds, context) == NK_WIDGET_INVALID) return false;
    bool dark = dark_theme(context);
    bool hover = nk_input_is_mouse_hovering_rect(&context->input, bounds);
    struct nk_command_buffer *canvas = nk_window_get_canvas(context);
    struct nk_color accent = ui_color(dark, 0x1677FF, 0x3C89E8);
    struct nk_color border = selected ? accent : ui_color(dark, 0xD9DEE9, 0x343C4C);
    struct nk_color text = ui_color(dark, 0x202431, 0xECEEF5);
    struct nk_color muted = ui_color(dark, 0x667085, 0xA7AFC0);
    nk_fill_rect(canvas, bounds, 8, ui_color(dark,
        hover ? 0xF7FAFF : 0xFFFFFF, hover ? 0x222936 : 0x1B1F29));
    nk_stroke_rect(canvas, bounds, 8, selected ? 2.0f : 1.0f, border);
    struct nk_rect preview = nk_rect(bounds.x + 8, bounds.y + 8, bounds.w - 16, 92);
    nk_fill_rect(canvas, preview, 6, ui_color(dark, 0xEEF6FF, 0x151D2A));
    draw_model_icon(canvas, entry->mode, preview, selected ? accent : muted);
    draw_text(context, canvas, nk_rect(bounds.x + 12, bounds.y + 108,
        bounds.w - 24, 24), entry->id, text);
    draw_text(context, canvas, nk_rect(bounds.x + 12, bounds.y + 134,
        bounds.w - 24, 22), mode_label(app, entry->mode), muted);
    struct nk_rect status = nk_rect(bounds.x + bounds.w - 94, bounds.y + 132, 82, 24);
    if (selected) {
        nk_fill_circle(canvas, nk_rect(status.x, status.y + 5, 13, 13), accent);
        draw_text(context, canvas, nk_rect(status.x + 19, status.y,
            status.w - 19, status.h), tr(app, "native.selected", "Selected"), accent);
    }
    struct nk_rect remove = nk_rect(bounds.x + bounds.w - 38, bounds.y + 8, 30, 30);
    bool remove_hover = !entry->preset &&
        nk_input_is_mouse_hovering_rect(&context->input, remove);
    if (!entry->preset) {
        nk_fill_rect(canvas, remove, 6, ui_color(dark,
            remove_hover ? 0xFFF1F0 : 0xFFFFFF, remove_hover ? 0x3A2023 : 0x252B38));
        struct nk_color danger = ui_color(dark, 0xCF1322, 0xFF7875);
        nk_stroke_line(canvas, remove.x + 9, remove.y + 9,
            remove.x + 21, remove.y + 21, 2, danger);
        nk_stroke_line(canvas, remove.x + 21, remove.y + 9,
            remove.x + 9, remove.y + 21, 2, danger);
    }
    if (remove_hover && nk_input_is_mouse_click_in_rect(&context->input,
        NK_BUTTON_LEFT, remove)) return confirm_remove(app, entry);
    if (nk_input_is_mouse_click_in_rect(&context->input, NK_BUTTON_LEFT, bounds) &&
        !selected) l2dcat_app_select_model(app, entry->id);
    return false;
}

static void behavior_rows(L2DCatApp *app, struct nk_context *context) {
    if (!app->config.model.behavior || !app->behaviors.count) return;
    l2dcat_pref_section(context, tr(app, "pages.preference.model.behaviorModal.title",
        "Motions and expressions"));
    for (size_t i = 0; i < app->behaviors.count; ++i) {
        L2DCatBehaviorEntry *entry = &app->behaviors.entries[i];
        L2DCatBehaviorShortcut *shortcut = behavior_shortcut(&app->config, entry->id);
        if (!shortcut) break;
        char id[L2DCAT_ID_CAP + 16];
        snprintf(id, sizeof(id), "behavior-%s", entry->id);
        l2dcat_pref_edit(context, id, entry->label, "", shortcut->shortcut,
            sizeof(shortcut->shortcut));
    }
}

void l2dcat_preferences_page_model(L2DCatApp *app, struct nk_context *context) {
    l2dcat_pref_section(context, tr(app, "pages.preference.model.title", "Installed models"));
    float width = nk_window_get_content_region(context).w;
    int columns = width >= 690 ? 3 : width >= 450 ? 2 : 1;
    nk_layout_row_dynamic(context, 168, columns);
    if (import_card(app, context))
        l2dcat_preferences_request_model_import(app->preferences);
    for (size_t i = 0; i < app->models.count; ++i)
        if (model_card(app, context, &app->models.entries[i])) break;
    behavior_rows(app, context);
}
