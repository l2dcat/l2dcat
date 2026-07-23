#include "preferences_state.h"
#include "preferences_controls.h"
#include "preferences_notice.h"
#include "ui_catime.h"
#include "l2dcat/file.h"
#include "l2dcat/path.h"
#include "l2dcat/tray.h"

#include <SDL3/SDL_opengl.h>
#include <math.h>
#include <stdio.h>

typedef struct RootStyle {
    struct nk_vec2 padding;
    struct nk_vec2 group_padding;
    struct nk_vec2 spacing;
    struct nk_style_item fixed;
    struct nk_color background;
    float group_border;
} RootStyle;

static const char *tr(const L2DCatPreferences *value, const char *key,
    const char *fallback) {
    return l2dcat_i18n_get(value->app->i18n, key, fallback);
}

static void draw_page(L2DCatPreferences *value, struct nk_context *context) {
    switch (value->page) {
    case 0: l2dcat_preferences_page_cat(value->app, context); break;
    case 1: l2dcat_preferences_page_general(value->app, context); break;
    case 2: l2dcat_preferences_page_model(value->app, context); break;
    case 3: l2dcat_preferences_page_shortcuts(value->app, context); break;
    default: l2dcat_preferences_page_about(value->app, context); break;
    }
}

static RootStyle root_style_save(struct nk_context *context) {
    RootStyle saved;
    saved.padding = context->style.window.padding;
    saved.group_padding = context->style.window.group_padding;
    saved.spacing = context->style.window.spacing;
    saved.fixed = context->style.window.fixed_background;
    saved.background = context->style.window.background;
    saved.group_border = context->style.window.group_border;
    return saved;
}

static void root_style_apply(struct nk_context *context,
    L2DCatUIPalette palette) {
    context->style.window.padding = nk_vec2(L2DCAT_UI_MARGIN,
        L2DCAT_UI_MARGIN);
    context->style.window.group_padding = nk_vec2(24, 8);
    context->style.window.spacing = nk_vec2(0, 0);
    context->style.window.fixed_background = nk_style_item_color(
        palette.background);
    context->style.window.background = palette.background;
    context->style.window.group_border = 0;
}

static void root_style_restore(struct nk_context *context,
    const RootStyle *saved) {
    context->style.window.padding = saved->padding;
    context->style.window.group_padding = saved->group_padding;
    context->style.window.spacing = saved->spacing;
    context->style.window.fixed_background = saved->fixed;
    context->style.window.background = saved->background;
    context->style.window.group_border = saved->group_border;
}

static bool draw_shell(L2DCatPreferences *value, struct nk_context *context,
    int width, int height, bool dark) {
    static const char *page_ids[] = {
        "page-cat", "page-general", "page-model", "page-shortcuts", "page-about"};
    const char *menus[] = {
        tr(value, "pages.preference.cat.title", "Cat"),
        tr(value, "pages.preference.general.title", "General"),
        tr(value, "pages.preference.model.title", "Model"),
        tr(value, "pages.preference.shortcut.title", "Shortcuts"),
        tr(value, "pages.preference.about.title", "About")};
    bool modal = l2dcat_preferences_remove_dialog_active(value->app);
    l2dcat_ui_shell_draw(context, (float)width, (float)height, dark);
    bool close_requested = l2dcat_ui_header(context, "Bongo Cat Neo",
        value->ui.heading_font, !modal, dark);
    l2dcat_ui_tabs(context, menus, 5, &value->page, !modal, dark);
    float body_height = (float)height - L2DCAT_UI_MARGIN * 2.0f -
        L2DCAT_UI_HEADER_HEIGHT - L2DCAT_UI_TABS_HEIGHT;
    nk_layout_row_dynamic(context, NK_MAX(120.0f, body_height), 1);
    L2DCatUIPalette p = l2dcat_ui_palette(dark);
    struct nk_rect body = nk_widget_bounds(context);
    struct nk_command_buffer *canvas = nk_window_get_canvas(context);
    nk_fill_rect(canvas, body, 16, p.background);
    nk_stroke_rect(canvas, body, 16, 1, p.border);
    struct nk_color clear = nk_rgba(0, 0, 0, 0);
    context->style.window.fixed_background = nk_style_item_color(clear);
    context->style.window.background = clear;
    context->style.window.group_padding = nk_vec2(22, 12);
    context->style.window.spacing = nk_vec2(10, 10);
    int scroll_page = NK_CLAMP(0, value->page, 4);
    if (value->scroll_ready[scroll_page])
        nk_group_set_scroll(context, page_ids[scroll_page], 0,
            (nk_uint)NK_MAX(0.0f, value->scroll_current[scroll_page]));
    if (nk_group_begin(context, page_ids[value->page], 0)) {
        draw_page(value, context);
        float wheel = context->input.mouse.scroll_delta.y;
        context->input.mouse.scroll_delta.y = 0;
        nk_group_end(context);
        nk_uint group_x = 0, group_y = 0;
        nk_group_get_scroll(context, page_ids[value->page], &group_x, &group_y);
        float actual = (float)group_y;
        if (!value->scroll_ready[scroll_page]) {
            value->scroll_ready[scroll_page] = true;
            value->scroll_current[scroll_page] = actual;
            value->scroll_target[scroll_page] = actual;
        }
        if (wheel != 0.0f)
            value->scroll_target[scroll_page] = NK_MAX(0.0f,
                actual - wheel * 72.0f);
        else if (fabsf(actual - value->scroll_current[scroll_page]) > 2.0f)
            value->scroll_target[scroll_page] = actual;
        float next = actual + (value->scroll_target[scroll_page] - actual) * .24f;
        if (fabsf(next - value->scroll_target[scroll_page]) < .5f)
            next = value->scroll_target[scroll_page];
        value->scroll_current[scroll_page] = next;
        if (wheel != 0.0f || fabsf(next - value->scroll_target[scroll_page]) > .5f)
            value->render_dirty = true;
    }
    context->style.window.fixed_background = nk_style_item_color(p.surface);
    context->style.window.background = p.surface;
    l2dcat_preferences_notice_draw(value, context, (float)width, (float)height);
    l2dcat_preferences_remove_dialog_draw(value->app, context);
    return close_requested;
}

static bool draw_frame(L2DCatPreferences *value, int width, int height,
    bool dark) {
    struct nk_context *context = &value->ui.context;
    RootStyle saved = root_style_save(context);
    L2DCatUIPalette palette = l2dcat_ui_palette(dark);
    root_style_apply(context, palette);
    bool close_requested = false;
    if (nk_begin(context, L2DCAT_NAME,
        nk_rect(0, 0, (float)width, (float)height), NK_WINDOW_NO_SCROLLBAR))
        close_requested = draw_shell(value, context, width, height, dark);
    nk_end(context);
    root_style_restore(context, &saved);
    return close_requested;
}

static void write_smoke_frame(L2DCatPreferences *value) {
    if (!value->app->smoke || value->frame_checked) return;
    value->frame_checked = true;
    char path[L2DCAT_PATH_CAP];
    l2dcat_path_join(path, sizeof(path), value->app->data_root, "ui-frame.txt");
    FILE *file = l2dcat_file_open(path, "wb");
    if (file) {
        fprintf(file, "valid=%d convert=%d vertices=%zu elements=%zu commands=%u "
            "draw_elements=%u gl_error=%u alpha_vertices=%u max_alpha=%u "
            "font_path=%d font_file=%d custom_font=%d font_probe=%d\n",
            l2dcat_ui_frame_valid(&value->ui), value->ui.last_convert_result,
            value->ui.last_vertex_bytes, value->ui.last_element_bytes,
            value->ui.last_draw_commands, value->ui.last_draw_elements,
            (unsigned)value->ui.last_gl_error, value->ui.nonzero_alpha_vertices,
            value->ui.max_alpha, value->ui.font_path_found,
            value->ui.font_file_loaded, value->ui.custom_font_loaded,
            value->ui.font_probe_loaded);
        fclose(file);
    }
    if (!l2dcat_ui_frame_valid(&value->ui)) value->app->exit_code = 1;
}

static bool reload_language(L2DCatPreferences *value) {
    if (value->font_language == value->app->config.app.language) return false;
    L2DCatError error = {0};
    if (!value->app->i18n || l2dcat_i18n_reload(value->app->i18n,
        value->app->config.app.language, &error) != L2DCAT_OK) {
        value->app->config.app.language = value->font_language;
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", error.message);
        return false;
    }
    value->font_language = value->app->config.app.language;
    if (value->app->tray) l2dcat_tray_sync(value->app->tray);
    value->render_dirty = true;
    return true;
}

void l2dcat_preferences_render(L2DCatPreferences *value) {
    if (!value || !value->window) return;
    uint64_t now = SDL_GetTicksNS();
    if (!value->render_dirty && value->last_render_ns) return;
    value->render_dirty = false;
    value->last_render_ns = now;
    l2dcat_preferences_input_end(value);
    SDL_GL_MakeCurrent(value->window, value->gl_context);
    l2dcat_preferences_apply_theme(value);
    int width, height;
    SDL_GetWindowSize(value->window, &width, &height);
    l2dcat_ui_cursor_begin(&value->ui);
    bool dark = l2dcat_preferences_resolved_theme(value) != 0;
    bool close_requested = draw_frame(value, width, height, dark);
    L2DCatUIPalette palette = l2dcat_ui_palette(dark);
    glClearColor(palette.background.r / 255.0f, palette.background.g / 255.0f,
        palette.background.b / 255.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    l2dcat_ui_render(&value->ui);
    SDL_GL_SwapWindow(value->window);
    write_smoke_frame(value);
    SDL_GL_MakeCurrent(value->app->window, value->app->gl_context);
    l2dcat_ui_cursor_apply(&value->ui);
    if (close_requested) {
        l2dcat_preferences_close(value);
        return;
    }
    if (value->import_requested && !l2dcat_preferences_import_is_open(
        value->import_dialog)) {
        value->import_requested = false;
        if (!l2dcat_preferences_import_open(value->import_dialog, value->window))
            value->render_dirty = true;
    }
    reload_language(value);
    if (l2dcat_pref_controls_animating(&value->ui.context))
        value->render_dirty = true;
}
