#include "runtime.h"
#include "l2dcat/audio.h"
#include "l2dcat/file.h"
#include "l2dcat/i18n.h"
#include "l2dcat/path.h"
#include "l2dcat/overlay.h"
#include "l2dcat/preferences.h"
#include "l2dcat/tray.h"
#include <SDL3/SDL_opengl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void parse_arguments(L2DCatApp *app, int argc, char **argv) {
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--ci-smoke") == 0) app->smoke = true;
        else if (strcmp(argv[i], "--ci-preferences") == 0) app->smoke_preferences = true;
        else if (strcmp(argv[i], "--ci-remove-imported") == 0) app->smoke_remove_imported = true;
        else if (strcmp(argv[i], "--ci-shortcuts") == 0) app->smoke_shortcuts = true;
        else if (strcmp(argv[i], "--ci-menu") == 0) app->smoke_menu = true;
        else if (strcmp(argv[i], "--ci-input-audit") == 0) app->smoke_input_audit = true;
        else if (strcmp(argv[i], "--ci-ignore-global-input") == 0)
            app->smoke_ignore_global_input = true;
        else if (strcmp(argv[i], "--ci-taskbar-visible") == 0)
            app->smoke_taskbar_visible = true;
        else if (strcmp(argv[i], "--ci-context-menu") == 0) app->smoke_context_menu = true;
        else if (strcmp(argv[i], "--ci-frame-series") == 0) app->smoke_frame_series = true;
        else if (strncmp(argv[i], "--ci-preference-page=", 21) == 0) {
            int page = atoi(argv[i] + 21);
            if (page >= 0 && page < 5) app->smoke_preference_page = page;
        }
        else if (strncmp(argv[i], "--ci-language=", 14) == 0) {
            const char *name = argv[i] + 14;
            for (int language = 0; language <= L2DCAT_LANG_VI_VN; ++language)
                if (strcmp(name, l2dcat_language_name((L2DCatLanguage)language)) == 0)
                    app->smoke_language = language;
        }
        else if (strncmp(argv[i], "--ci-theme=", 11) == 0) {
            const char *name = argv[i] + 11;
            for (int theme = 0; theme <= L2DCAT_THEME_DARK; ++theme)
                if (strcmp(name, l2dcat_theme_name((L2DCatTheme)theme)) == 0)
                    app->smoke_theme = theme;
        }
        else if (strncmp(argv[i], "--ci-model=", 11) == 0)
            snprintf(app->smoke_model, sizeof(app->smoke_model), "%s", argv[i] + 11);
        else if (strncmp(argv[i], "--ci-live2d-scenario=", 21) == 0)
            snprintf(app->smoke_live2d_scenario, sizeof(app->smoke_live2d_scenario),
                "%s", argv[i] + 21);
        else if (strncmp(argv[i], "--ci-exit-ms=", 13) == 0) {
            uint64_t delay = strtoull(argv[i] + 13, NULL, 10);
            app->smoke_deadline_ns = delay * 1000000ull;
        } else if (strncmp(argv[i], "--config=", 9) == 0) {
            snprintf(app->config_path, sizeof(app->config_path), "%s", argv[i] + 9);
        } else if (strncmp(argv[i], "--data-root=", 12) == 0) {
            snprintf(app->data_root, sizeof(app->data_root), "%s", argv[i] + 12);
        } else if (strncmp(argv[i], "--ci-import=", 12) == 0) {
            snprintf(app->smoke_import_path, sizeof(app->smoke_import_path), "%s", argv[i] + 12);
        }
    }
    if (app->smoke && !app->smoke_deadline_ns) app->smoke_deadline_ns = 1500000000ull;
}

static void locate_config(L2DCatApp *app) {
    if (!app->data_root[0]) {
        char *pref = SDL_GetPrefPath("l2dcat", L2DCAT_NAME);
        if (!pref) return;
        snprintf(app->data_root, sizeof(app->data_root), "%s", pref);
        SDL_free(pref);
    }
    SDL_CreateDirectory(app->data_root);
    if (!app->config_path[0]) l2dcat_path_join(app->config_path,
        sizeof(app->config_path), app->data_root, "settings.json");
}

static void ci_failure(L2DCatApp *app, const L2DCatError *error) {
    app->exit_code = 1;
    if (!app->data_root[0]) return;
    char path[L2DCAT_PATH_CAP];
    l2dcat_path_join(path, sizeof(path), app->data_root, "ci-error.log");
    FILE *file = l2dcat_file_open(path, "wb");
    if (!file) return;
    fputs(error && error->message[0] ? error->message : "CI operation failed", file);
    fclose(file);
}

static void scan_models(L2DCatApp *app) {
    L2DCatError error = {0};
    char path[L2DCAT_PATH_CAP];
    l2dcat_path_join(path, sizeof(path), app->asset_root, "models");
    l2dcat_models_scan(&app->models, path, true, &error);
    if (app->data_root[0]) {
        l2dcat_path_join(path, sizeof(path), app->data_root, "custom-models");
        l2dcat_models_scan(&app->models, path, false, &error);
    }
}

static void load_selected_model(L2DCatApp *app) {
    const L2DCatModelEntry *entry = l2dcat_models_find(&app->models, app->config.current_model);
    if (!entry && app->models.count) entry = &app->models.entries[0];
    if (!entry) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "No model3 directory found");
        return;
    }
    l2dcat_app_select_model(app, entry->id);
}

static bool initialize(L2DCatApp *app, int argc, char **argv, L2DCatError *error) {
    memset(app, 0, sizeof(*app));
    app->smoke_language = -1;
    app->smoke_theme = -1;
    app->smoke_preference_page = -1;
    l2dcat_config_defaults(&app->config);
    l2dcat_input_init(&app->input);
    l2dcat_shortcut_init(&app->shortcut_state);
    l2dcat_models_init(&app->models);
    parse_arguments(app, argc, argv);
    locate_config(app);
    if (app->config_path[0]) {
        L2DCatResult loaded = l2dcat_config_load(app->config_path, &app->config, error);
        if (loaded != L2DCAT_OK) SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "%s", error->message);
    }
    if (app->smoke_language >= 0)
        app->config.app.language = (L2DCatLanguage)app->smoke_language;
    if (app->smoke_theme >= 0)
        app->config.app.theme = (L2DCatTheme)app->smoke_theme;
    if (app->smoke_taskbar_visible) app->config.window.taskbar_visible = true;
    if (app->smoke_model[0])
        snprintf(app->config.current_model, sizeof(app->config.current_model),
            "%s", app->smoke_model);
    if (l2dcat_window_create(app, error) != L2DCAT_OK) return false;
    l2dcat_app_locate_assets(app);
    app->i18n = l2dcat_i18n_create(app->locale_root, app->config.app.language, error);
    if (!app->i18n) SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "%s", error->message);
    if (l2dcat_platform_init(&app->platform, app->window, &app->input, error) != L2DCAT_OK) return false;
    l2dcat_window_apply(app);
    app->live2d = l2dcat_live2d_create(app->asset_root, error);
    if (!app->live2d) return false;
    app->overlay = l2dcat_overlay_create(error);
    if (!app->overlay) return false;
    app->audio = l2dcat_audio_create(error);
    if (!app->audio) SDL_LogWarn(SDL_LOG_CATEGORY_AUDIO, "%s", error->message);
    else l2dcat_audio_set_enabled(app->audio, app->config.model.motion_sound);
    scan_models(app);
    load_selected_model(app);
    if (app->smoke_import_path[0]) {
        L2DCatError import_error = {0};
        if (l2dcat_app_import_model(app, app->smoke_import_path, &import_error) != L2DCAT_OK) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", import_error.message);
            ci_failure(app, &import_error);
        } else if (app->smoke_remove_imported) {
            char imported[L2DCAT_PATH_CAP];
            snprintf(imported, sizeof(imported), "%s", app->config.current_model);
            if (l2dcat_app_remove_model(app, imported, &import_error) != L2DCAT_OK)
                ci_failure(app, &import_error);
        }
    }
    app->running = true;
    app->tray = l2dcat_tray_create(app, error);
    if (!app->tray && app->config.app.tray_visible)
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "%s", error->message);
    app->preferences = l2dcat_preferences_create(app);
    if (app->smoke_context_menu) l2dcat_window_show_context_menu(app);
    l2dcat_live2d_audit_run(app);
    if (app->smoke_shortcuts && !l2dcat_app_shortcuts_self_test(app)) {
        L2DCatError shortcut_error = {0};
        l2dcat_error_set(&shortcut_error, L2DCAT_ERROR_PLATFORM, "Shortcut action self-test failed");
        ci_failure(app, &shortcut_error);
    }
    if (app->smoke_menu && (!l2dcat_window_menu_self_test(app) ||
        !l2dcat_window_geometry_self_test(app) || !l2dcat_window_wheel_self_test(app) || !l2dcat_tray_self_test(app->tray))) {
        L2DCatError menu_error = {0};
        l2dcat_error_set(&menu_error, L2DCAT_ERROR_PLATFORM, "Context menu action self-test failed");
        ci_failure(app, &menu_error);
    }
    if (app->smoke_preferences) l2dcat_preferences_show(app->preferences);
    app->last_frame_ns = SDL_GetTicksNS();
    app->dirty = true;
    if (app->smoke_deadline_ns) app->smoke_deadline_ns += app->last_frame_ns;
    return true;
}

static void handle_event(L2DCatApp *app, const SDL_Event *event) {
    if (l2dcat_preferences_event(app->preferences, event)) return;
    if (!l2dcat_window_event(app, event)) app->running = false;
    if (event->type >= SDL_EVENT_GAMEPAD_AXIS_MOTION &&
        event->type <= SDL_EVENT_GAMEPAD_TOUCHPAD_UP) l2dcat_gamepad_event(app, event);
}

static void drain_input(L2DCatApp *app) {
    L2DCatInputEvent event;
    while (l2dcat_input_pop(&app->input, &event)) {
        if (app->smoke_ignore_global_input) continue;
        if (app->smoke_input_audit) {
            char path[L2DCAT_PATH_CAP];
            l2dcat_path_join(path, sizeof(path), app->data_root, "input-audit.txt");
            FILE *file = l2dcat_file_open(path, "ab");
            if (file) { fprintf(file, "kind=%d name=%s value=%.3f\n",
                event.kind, event.name, event.value); fclose(file); }
        }
        uint64_t delay = strcmp(event.name, "CapsLock") == 0 ? 100 : 0;
#ifdef _WIN32
        if (event.kind == L2DCAT_INPUT_KEY_DOWN && !delay)
            delay = (uint64_t)(app->config.model.auto_release_seconds * 1000.0f);
#endif
        l2dcat_input_auto_release(&app->input, &event, delay);
        l2dcat_app_shortcuts(app, &event);
        l2dcat_app_apply_input(app, &event);
    }
    uint64_t now = SDL_GetTicks();
    while (l2dcat_input_take_release(&app->input, now, &event)) {
        l2dcat_app_shortcuts(app, &event);
        l2dcat_app_apply_input(app, &event);
    }
    if (!app->smoke_ignore_global_input) l2dcat_app_apply_mouse(app);
}

static void update_model(L2DCatApp *app, uint64_t now) {
    float elapsed = (float)((now - app->last_frame_ns) / 1000000000.0);
    if (elapsed > 0.25f) elapsed = 0.25f;
    app->last_frame_ns = now;
    if (l2dcat_live2d_update(app->live2d, elapsed)) app->dirty = true;
}

static void render(L2DCatApp *app) {
    SDL_GL_MakeCurrent(app->window, app->gl_context);
    int width, height;
    SDL_GetWindowSizeInPixels(app->window, &width, &height);
    glViewport(0, 0, width, height);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    l2dcat_overlay_begin_clip(app->overlay, app->config.window.radius_percent);
    l2dcat_overlay_draw_background(app->overlay, app->config.model.mirror);
    l2dcat_live2d_set_mirror(app->live2d, app->config.model.mirror);
    l2dcat_live2d_draw(app->live2d);
    l2dcat_overlay_draw_keys(app->overlay, app->config.model.mirror);
    l2dcat_overlay_end_clip(app->overlay);
    l2dcat_frame_audit(app, width, height);
    SDL_GL_SwapWindow(app->window);
    app->dirty = false;
    l2dcat_window_sync_click_through(app); l2dcat_window_schedule_hit_check(app);
}

static void loop(L2DCatApp *app) {
    while (app->running) {
        bool preferences = l2dcat_preferences_visible(app->preferences);
        int wait_ms = preferences ? 16 : app->config.window.visible
            ? L2DCAT_FRAME_WAIT(app) : 250;
        if (app->wheel_animation_active && wait_ms > 16) wait_ms = 16;
        l2dcat_preferences_input_begin(app->preferences);
        SDL_Event event;
        if (SDL_WaitEventTimeout(&event, wait_ms)) {
            handle_event(app, &event);
            while (SDL_PollEvent(&event)) handle_event(app, &event);
        }
        l2dcat_preferences_input_end(app->preferences);
        drain_input(app);
        uint64_t now = SDL_GetTicksNS(); l2dcat_window_update_wheel_animation(app, now);
        l2dcat_window_apply_pending_resize(app);
        l2dcat_app_update_hover(app, now);
        if (app->config.window.visible) update_model(app, now); else app->last_frame_ns = now;
        if (app->config.window.visible && app->dirty) render(app);
        l2dcat_preferences_render(app->preferences);
        l2dcat_tray_sync(app->tray);
        if (app->smoke_deadline_ns && now >= app->smoke_deadline_ns) app->running = false;
    }
}
static void shutdown(L2DCatApp *app) {
    if (!app->smoke && app->config_path[0]) {
        L2DCatError error = {0};
        if (l2dcat_config_save(app->config_path, &app->config, &error) != L2DCAT_OK)
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", error.message);
    }
    l2dcat_preferences_destroy(app->preferences);
    l2dcat_i18n_destroy(app->i18n);
    l2dcat_tray_destroy(app->tray);
    l2dcat_audio_destroy(app->audio);
    l2dcat_overlay_destroy(app->overlay);
    l2dcat_live2d_destroy(app->live2d);
    l2dcat_platform_shutdown(&app->platform);
    l2dcat_window_destroy(app);
}

int l2dcat_app_run(int argc, char **argv) {
    if (!l2dcat_platform_single_instance_begin()) return 0;
    L2DCatApp *app = calloc(1, sizeof(*app));
    if (!app) { l2dcat_platform_single_instance_end(); return 1; }
    L2DCatError error = {0};
    if (!initialize(app, argc, argv, &error)) {
        fprintf(stderr, "%s\n", error.message[0] ? error.message : "Initialization failed");
        if (app->smoke) ci_failure(app, &error);
        shutdown(app); free(app);
        l2dcat_platform_single_instance_end();
        return 1;
    }
    loop(app);
    shutdown(app);
    bool restart = app->restart_requested; int exit_code = app->exit_code;
    free(app); l2dcat_platform_single_instance_end();
    if (restart) l2dcat_platform_restart();
    return exit_code;
}
