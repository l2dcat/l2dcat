#include "bongo/preferences.h"
#include "bongo/app.h"
#include "bongo/i18n.h"
#include "bongo/file.h"
#include "bongo/path.h"
#include "preferences_internal.h"
#include "ui_backend.h"
#include "ui_font.h"
#include "ui_sidebar.h"
#include "ui_theme.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <stdio.h>
#include <stdlib.h>
struct BongoPreferences {
    BongoApp *app;
    SDL_Window *window;
    SDL_GLContext gl_context;
    bool owns_gl_context;
    BongoUIBackend ui;
    int page;
    int style_theme;
    BongoLanguage font_language;
    nk_rune glyph_ranges[2048];
    bool input_active, import_requested;
    bool import_dialog_open;
    bool frame_checked;
};
static const char *tr(const BongoPreferences *value, const char *key,
    const char *fallback) {
    return bongo_i18n_get(value->app->i18n, key, fallback);
}
static int resolved_theme(const BongoPreferences *value) {
    if (value->app->config.app.theme == BONGO_THEME_DARK) return 1;
    if (value->app->config.app.theme == BONGO_THEME_LIGHT) return 0;
    return SDL_GetSystemTheme() == SDL_SYSTEM_THEME_DARK;
}
static void apply_theme(BongoPreferences *value) {
    int dark = resolved_theme(value);
    if (dark == value->style_theme) return;
    value->style_theme = dark;
    bongo_ui_apply_theme(&value->ui.context, dark != 0);
}
static bool open_window(BongoPreferences *value) {
    SDL_WindowFlags flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE |
        SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_HIDDEN;
    value->window = SDL_CreateWindow(tr(value, "pages.preference.title", "Preferences"),
        900, 640, flags);
    if (!value->window) return false;
    SDL_SetWindowMinimumSize(value->window, 720, 520);
    value->gl_context = value->app->gl_context;
    if (!SDL_GL_MakeCurrent(value->window, value->gl_context)) {
        SDL_GL_MakeCurrent(value->app->window, value->app->gl_context);
        SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 1);
        value->gl_context = SDL_GL_CreateContext(value->window);
        SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 0);
        value->owns_gl_context = value->gl_context != NULL;
        if (!value->gl_context ||
            !SDL_GL_MakeCurrent(value->window, value->gl_context)) return false;
    }
    BongoError error = {0};
    char font_path[BONGO_PATH_CAP];
    const char *font;
    const nk_rune *ranges = NULL;
    bool multilingual = value->app->config.app.language == BONGO_LANG_ZH_CN ||
        value->app->config.app.language == BONGO_LANG_ZH_TW;
    font = bongo_ui_system_font(font_path, sizeof(font_path), multilingual);
    if (value->app->config.app.language != BONGO_LANG_EN_US && value->app->i18n) {
        bongo_i18n_glyph_ranges(value->app->i18n, value->glyph_ranges,
            sizeof(value->glyph_ranges) / sizeof(value->glyph_ranges[0]));
        ranges = value->glyph_ranges;
    }
    if (!bongo_ui_init(&value->ui, value->window, font, ranges, &error)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", error.message);
        return false;
    }
    value->style_theme = -1;
    value->font_language = value->app->config.app.language;
    SDL_GL_SetSwapInterval(1);
    SDL_StartTextInput(value->window);
    SDL_ShowWindow(value->window);
    SDL_RaiseWindow(value->window);
    SDL_GL_MakeCurrent(value->app->window, value->app->gl_context);
    return true;
}
BongoPreferences *bongo_preferences_create(BongoApp *app) {
    if (!app) return NULL;
    BongoPreferences *value = calloc(1, sizeof(*value));
    if (value) value->app = app;
    if (value && app->smoke_preference_page >= 0)
        value->page = app->smoke_preference_page;
    return value;
}

void bongo_preferences_show(BongoPreferences *value) {
    if (!value) return;
    if (!value->window && !open_window(value)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Preferences failed: %s", SDL_GetError());
        bongo_preferences_close(value);
        return;
    }
    SDL_ShowWindow(value->window);
    SDL_RaiseWindow(value->window);
}

void bongo_preferences_close(BongoPreferences *value) {
    if (!value || !value->window) return;
    SDL_GL_MakeCurrent(value->window, value->gl_context);
    if (value->input_active) bongo_preferences_input_end(value);
    SDL_StopTextInput(value->window);
    bongo_ui_destroy(&value->ui);
    if (value->owns_gl_context && value->gl_context)
        SDL_GL_DestroyContext(value->gl_context);
    SDL_DestroyWindow(value->window);
    value->window = NULL;
    value->gl_context = NULL;
    value->owns_gl_context = false;
    SDL_GL_MakeCurrent(value->app->window, value->app->gl_context);
    BongoError error = {0};
    if (!value->app->smoke && value->app->config_path[0] &&
        bongo_config_save(value->app->config_path,
        &value->app->config, &error) != BONGO_OK)
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", error.message);
}

void bongo_preferences_destroy(BongoPreferences *value) {
    if (!value) return;
    bongo_preferences_close(value);
    free(value);
}

void bongo_preferences_request_model_import(BongoPreferences *value) {
    if (value && !value->import_dialog_open) value->import_requested = true;
}

static void SDLCALL model_imported(void *userdata, const char *const *files,
    int filter) {
    (void)filter;
    BongoPreferences *value = userdata;
    value->import_dialog_open = false;
    if (!files || !files[0]) return;
    bongo_preferences_import_path(value->app, value->window, files[0]);
}

bool bongo_preferences_visible(const BongoPreferences *value) {
    return value && value->window;
}

void bongo_preferences_input_begin(BongoPreferences *value) {
    if (!value || !value->window || value->input_active) return;
    bongo_ui_input_begin(&value->ui);
    value->input_active = true;
}

void bongo_preferences_input_end(BongoPreferences *value) {
    if (!value || !value->window || !value->input_active) return;
    bongo_ui_input_end(&value->ui);
    value->input_active = false;
}

static Uint32 event_window(const SDL_Event *event) {
    if (event->type >= SDL_EVENT_WINDOW_FIRST && event->type <= SDL_EVENT_WINDOW_LAST)
        return event->window.windowID;
    switch (event->type) {
    case SDL_EVENT_KEY_DOWN: case SDL_EVENT_KEY_UP: return event->key.windowID;
    case SDL_EVENT_TEXT_INPUT: return event->text.windowID;
    case SDL_EVENT_MOUSE_MOTION: return event->motion.windowID;
    case SDL_EVENT_MOUSE_BUTTON_DOWN: case SDL_EVENT_MOUSE_BUTTON_UP:
        return event->button.windowID;
    case SDL_EVENT_MOUSE_WHEEL: return event->wheel.windowID;
    case SDL_EVENT_DROP_FILE: return event->drop.windowID;
    default: return 0;
    }
}

bool bongo_preferences_event(BongoPreferences *value, const SDL_Event *event) {
    if (!value || !value->window || !event ||
        event_window(event) != SDL_GetWindowID(value->window)) return false;
    if (event->type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
        bongo_preferences_close(value);
        return true;
    }
    if (event->type == SDL_EVENT_DROP_FILE) {
        bongo_preferences_import_path(value->app, value->window, event->drop.data);
        return true;
    }
    if (!value->input_active) bongo_preferences_input_begin(value);
    bongo_ui_event(&value->ui, event);
    return true;
}

static void draw_page(BongoPreferences *value, struct nk_context *context) {
    switch (value->page) {
    case 0: bongo_preferences_page_cat(value->app, context); break;
    case 1: bongo_preferences_page_general(value->app, context); break;
    case 2: bongo_preferences_page_model(value->app, context); break;
    case 3: bongo_preferences_page_shortcuts(value->app, context); break;
    default: bongo_preferences_page_about(value->app, context); break;
    }
}

void bongo_preferences_render(BongoPreferences *value) {
    if (!value || !value->window) return;
    bongo_preferences_input_end(value);
    SDL_GL_MakeCurrent(value->window, value->gl_context);
    apply_theme(value);
    int width, height;
    SDL_GetWindowSize(value->window, &width, &height);
    struct nk_context *context = &value->ui.context;
    bool dark = resolved_theme(value) != 0;
    struct nk_vec2 old_padding = context->style.window.padding;
    struct nk_vec2 old_group_padding = context->style.window.group_padding;
    struct nk_vec2 old_spacing = context->style.window.spacing;
    struct nk_style_item old_fixed = context->style.window.fixed_background;
    struct nk_color old_background = context->style.window.background;
    float old_group_border = context->style.window.group_border;
    context->style.window.padding = nk_vec2(0, 0);
    context->style.window.spacing = nk_vec2(0, 0);
    if (nk_begin(context, "Preferences", nk_rect(0, 0, (float)width, (float)height),
        NK_WINDOW_NO_SCROLLBAR)) {
        const char *menus[] = {
            tr(value, "pages.preference.cat.title", "Cat"),
            tr(value, "pages.preference.general.title", "General"),
            tr(value, "pages.preference.model.title", "Model"),
            tr(value, "pages.preference.shortcut.title", "Shortcuts"),
            tr(value, "pages.preference.about.title", "About")};
        nk_layout_row_begin(context, NK_STATIC, (float)height, 2);
        nk_layout_row_push(context, 184);
        struct nk_color sidebar = dark ? nk_rgb(15, 20, 29) : nk_rgb(248, 250, 253);
        context->style.window.fixed_background = nk_style_item_color(sidebar);
        context->style.window.background = sidebar;
        context->style.window.group_padding = nk_vec2(12, 12);
        context->style.window.group_border = 0;
        if (nk_group_begin(context, "sidebar", NK_WINDOW_NO_SCROLLBAR)) {
            bongo_ui_sidebar(context, menus, &value->page, dark);
            nk_group_end(context);
        }
        nk_layout_row_push(context, (float)width - 184);
        struct nk_color content = dark ? nk_rgb(13, 17, 24) : nk_rgb(242, 245, 249);
        context->style.window.fixed_background = nk_style_item_color(content);
        context->style.window.background = content;
        context->style.window.group_padding = nk_vec2(24, 20);
        if (nk_group_begin(context, "page", 0)) {
            nk_layout_row_dynamic(context, 38, 1); nk_label(context,
                menus[value->page], NK_TEXT_LEFT);
            nk_layout_row_dynamic(context, 8, 1); nk_spacing(context, 1);
            draw_page(value, context);
            nk_group_end(context);
        }
        nk_layout_row_end(context);
    }
    nk_end(context);
    context->style.window.padding = old_padding;
    context->style.window.group_padding = old_group_padding;
    context->style.window.spacing = old_spacing;
    context->style.window.fixed_background = old_fixed;
    context->style.window.background = old_background;
    context->style.window.group_border = old_group_border;
    if (resolved_theme(value)) glClearColor(0.067f, 0.078f, 0.106f, 1.0f);
    else glClearColor(0.961f, 0.969f, 0.984f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    bongo_ui_render(&value->ui);
    SDL_GL_SwapWindow(value->window);
    if (value->app->smoke && !value->frame_checked) {
        value->frame_checked = true; char path[BONGO_PATH_CAP];
        bongo_path_join(path, sizeof(path), value->app->data_root, "ui-frame.txt");
        FILE *file = bongo_file_open(path, "wb");
        if (file) {
            fprintf(file, "valid=%d convert=%d vertices=%zu elements=%zu commands=%u "
                "draw_elements=%u gl_error=%u alpha_vertices=%u max_alpha=%u "
                "font_path=%d font_file=%d custom_font=%d font_probe=%d\n",
                bongo_ui_frame_valid(&value->ui),
                value->ui.last_convert_result, value->ui.last_vertex_bytes,
                value->ui.last_element_bytes, value->ui.last_draw_commands,
                value->ui.last_draw_elements, (unsigned)value->ui.last_gl_error,
                value->ui.nonzero_alpha_vertices, value->ui.max_alpha,
                value->ui.font_path_found, value->ui.font_file_loaded,
                value->ui.custom_font_loaded, value->ui.font_probe_loaded);
            fclose(file);
        }
        if (!bongo_ui_frame_valid(&value->ui)) value->app->exit_code = 1;
    }
    SDL_GL_MakeCurrent(value->app->window, value->app->gl_context);
    if (value->import_requested && !value->import_dialog_open) {
        value->import_requested = false;
        value->import_dialog_open = true;
        SDL_ShowOpenFolderDialog(model_imported, value, value->window, NULL, false);
    }
    if (value->font_language != value->app->config.app.language) {
        BongoError error = {0};
        if (!value->app->i18n || bongo_i18n_reload(value->app->i18n,
            value->app->config.app.language, &error) != BONGO_OK) {
            value->app->config.app.language = value->font_language;
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", error.message);
            return;
        }
        bongo_preferences_close(value);
        bongo_preferences_show(value);
    }
}
