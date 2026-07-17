#include "runtime.h"
#include "bongo/audio.h"
#include "bongo/file.h"
#include "bongo/i18n.h"
#include "bongo/path.h"
#include "bongo/overlay.h"
#include "bongo/preferences.h"
#include "bongo/tray.h"

#include <SDL3/SDL_opengl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void parse_arguments(BongoApp *app, int argc, char **argv) {
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
        else if (strncmp(argv[i], "--ci-preference-page=", 21) == 0) {
            int page = atoi(argv[i] + 21);
            if (page >= 0 && page < 5) app->smoke_preference_page = page;
        }
        else if (strncmp(argv[i], "--ci-language=", 14) == 0) {
            const char *name = argv[i] + 14;
            for (int language = 0; language <= BONGO_LANG_VI_VN; ++language)
                if (strcmp(name, bongo_language_name((BongoLanguage)language)) == 0)
                    app->smoke_language = language;
        }
        else if (strncmp(argv[i], "--ci-theme=", 11) == 0) {
            const char *name = argv[i] + 11;
            for (int theme = 0; theme <= BONGO_THEME_DARK; ++theme)
                if (strcmp(name, bongo_theme_name((BongoTheme)theme)) == 0)
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

static void locate_config(BongoApp *app) {
    if (!app->data_root[0]) {
        char *pref = SDL_GetPrefPath("BongoCatNative", BONGO_NAME);
        if (!pref) return;
        snprintf(app->data_root, sizeof(app->data_root), "%s", pref);
        SDL_free(pref);
    }
    SDL_CreateDirectory(app->data_root);
    if (!app->config_path[0]) bongo_path_join(app->config_path,
        sizeof(app->config_path), app->data_root, "settings.json");
}

static void ci_failure(BongoApp *app, const BongoError *error) {
    app->exit_code = 1;
    if (!app->data_root[0]) return;
    char path[BONGO_PATH_CAP];
    bongo_path_join(path, sizeof(path), app->data_root, "ci-error.log");
    FILE *file = bongo_file_open(path, "wb");
    if (!file) return;
    fputs(error && error->message[0] ? error->message : "CI operation failed", file);
    fclose(file);
}

static void scan_models(BongoApp *app) {
    BongoError error = {0};
    char path[BONGO_PATH_CAP];
    bongo_path_join(path, sizeof(path), app->asset_root, "models");
    bongo_models_scan(&app->models, path, true, &error);
    if (app->data_root[0]) {
        bongo_path_join(path, sizeof(path), app->data_root, "custom-models");
        bongo_models_scan(&app->models, path, false, &error);
    }
}

static void load_selected_model(BongoApp *app) {
    const BongoModelEntry *entry = bongo_models_find(&app->models, app->config.current_model);
    if (!entry && app->models.count) entry = &app->models.entries[0];
    if (!entry) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "No model3 directory found");
        return;
    }
    bongo_app_select_model(app, entry->id);
}

static bool initialize(BongoApp *app, int argc, char **argv, BongoError *error) {
    memset(app, 0, sizeof(*app));
    app->smoke_language = -1;
    app->smoke_theme = -1;
    app->smoke_preference_page = -1;
    bongo_config_defaults(&app->config);
    bongo_input_init(&app->input);
    bongo_shortcut_init(&app->shortcut_state);
    bongo_models_init(&app->models);
    parse_arguments(app, argc, argv);
    locate_config(app);
    if (app->config_path[0]) {
        BongoResult loaded = bongo_config_load(app->config_path, &app->config, error);
        if (loaded != BONGO_OK) SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "%s", error->message);
    }
    if (app->smoke_language >= 0)
        app->config.app.language = (BongoLanguage)app->smoke_language;
    if (app->smoke_theme >= 0)
        app->config.app.theme = (BongoTheme)app->smoke_theme;
    if (app->smoke_taskbar_visible) app->config.window.taskbar_visible = true;
    if (app->smoke_model[0])
        snprintf(app->config.current_model, sizeof(app->config.current_model),
            "%s", app->smoke_model);
    if (bongo_window_create(app, error) != BONGO_OK) return false;
    bongo_app_locate_assets(app);
    app->i18n = bongo_i18n_create(app->locale_root, app->config.app.language, error);
    if (!app->i18n) SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "%s", error->message);
    if (bongo_platform_init(&app->platform, app->window, &app->input, error) != BONGO_OK) return false;
    bongo_window_apply(app);
    app->live2d = bongo_live2d_create(error);
    if (!app->live2d) return false;
    app->overlay = bongo_overlay_create(error);
    if (!app->overlay) return false;
    app->audio = bongo_audio_create(error);
    if (!app->audio) SDL_LogWarn(SDL_LOG_CATEGORY_AUDIO, "%s", error->message);
    else bongo_audio_set_enabled(app->audio, app->config.model.motion_sound);
    scan_models(app);
    load_selected_model(app);
    if (app->smoke_import_path[0]) {
        BongoError import_error = {0};
        if (bongo_app_import_model(app, app->smoke_import_path, &import_error) != BONGO_OK) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", import_error.message);
            ci_failure(app, &import_error);
        } else if (app->smoke_remove_imported) {
            char imported[BONGO_PATH_CAP];
            snprintf(imported, sizeof(imported), "%s", app->config.current_model);
            if (bongo_app_remove_model(app, imported, &import_error) != BONGO_OK)
                ci_failure(app, &import_error);
        }
    }
    app->running = true;
    app->tray = bongo_tray_create(app, error);
    if (!app->tray && app->config.app.tray_visible)
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "%s", error->message);
    app->preferences = bongo_preferences_create(app);
    if (app->smoke_context_menu) bongo_window_show_context_menu(app);
    bongo_live2d_audit_run(app);
    if (app->smoke_shortcuts && !bongo_app_shortcuts_self_test(app)) {
        BongoError shortcut_error = {0};
        bongo_error_set(&shortcut_error, BONGO_ERROR_PLATFORM, "Shortcut action self-test failed");
        ci_failure(app, &shortcut_error);
    }
    if (app->smoke_menu && (!bongo_window_menu_self_test(app) ||
        !bongo_window_geometry_self_test(app) || !bongo_tray_self_test(app->tray))) {
        BongoError menu_error = {0};
        bongo_error_set(&menu_error, BONGO_ERROR_PLATFORM, "Context menu action self-test failed");
        ci_failure(app, &menu_error);
    }
    if (app->smoke_preferences) bongo_preferences_show(app->preferences);
    app->last_frame_ns = SDL_GetTicksNS();
    app->dirty = true;
    if (app->smoke_deadline_ns) app->smoke_deadline_ns += app->last_frame_ns;
    return true;
}

static void handle_event(BongoApp *app, const SDL_Event *event) {
    if (bongo_preferences_event(app->preferences, event)) return;
    if (!bongo_window_event(app, event)) app->running = false;
    if (event->type >= SDL_EVENT_GAMEPAD_AXIS_MOTION &&
        event->type <= SDL_EVENT_GAMEPAD_TOUCHPAD_UP) bongo_gamepad_event(app, event);
}

static void drain_input(BongoApp *app) {
    BongoInputEvent event;
    while (bongo_input_pop(&app->input, &event)) {
        if (app->smoke_ignore_global_input) continue;
        if (app->smoke_input_audit) {
            char path[BONGO_PATH_CAP];
            bongo_path_join(path, sizeof(path), app->data_root, "input-audit.txt");
            FILE *file = bongo_file_open(path, "ab");
            if (file) { fprintf(file, "kind=%d name=%s value=%.3f\n",
                event.kind, event.name, event.value); fclose(file); }
        }
        uint64_t delay = strcmp(event.name, "CapsLock") == 0 ? 100 : 0;
#ifdef _WIN32
        if (event.kind == BONGO_INPUT_KEY_DOWN && !delay)
            delay = (uint64_t)(app->config.model.auto_release_seconds * 1000.0f);
#endif
        bongo_input_auto_release(&app->input, &event, delay);
        bongo_app_shortcuts(app, &event);
        bongo_app_apply_input(app, &event);
    }
    uint64_t now = SDL_GetTicks();
    while (bongo_input_take_release(&app->input, now, &event)) {
        bongo_app_shortcuts(app, &event);
        bongo_app_apply_input(app, &event);
    }
    bongo_app_apply_mouse(app);
}

static void update_model(BongoApp *app, uint64_t now) {
    float elapsed = (float)((now - app->last_frame_ns) / 1000000000.0);
    if (elapsed > 0.25f) elapsed = 0.25f;
    app->last_frame_ns = now;
    if (bongo_live2d_update(app->live2d, elapsed)) app->dirty = true;
}

static void render(BongoApp *app) {
    SDL_GL_MakeCurrent(app->window, app->gl_context);
    int width, height;
    SDL_GetWindowSizeInPixels(app->window, &width, &height);
    glViewport(0, 0, width, height);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    bongo_overlay_begin_clip(app->overlay, app->config.window.radius_percent);
    bongo_overlay_draw_background(app->overlay, app->config.model.mirror);
    bongo_live2d_set_mirror(app->live2d, app->config.model.mirror);
    bongo_live2d_draw(app->live2d);
    bongo_overlay_draw_keys(app->overlay, app->config.model.mirror);
    bongo_overlay_end_clip(app->overlay);
    bongo_frame_audit(app, width, height);
    SDL_GL_SwapWindow(app->window);
    app->dirty = false;
}

static void loop(BongoApp *app) {
    while (app->running) {
        bool preferences = bongo_preferences_visible(app->preferences);
        int wait_ms = preferences ? 16 : app->config.window.visible
            ? BONGO_FRAME_WAIT(app) : 250;
        bongo_preferences_input_begin(app->preferences);
        SDL_Event event;
        if (SDL_WaitEventTimeout(&event, wait_ms)) {
            handle_event(app, &event);
            while (SDL_PollEvent(&event)) handle_event(app, &event);
        }
        bongo_preferences_input_end(app->preferences);
        drain_input(app);
        uint64_t now = SDL_GetTicksNS();
        bongo_app_update_hover(app, now);
        if (app->config.window.visible) update_model(app, now);
        else app->last_frame_ns = now;
        if (app->config.window.visible && app->dirty) render(app);
        bongo_preferences_render(app->preferences);
        bongo_tray_sync(app->tray);
        if (app->smoke_deadline_ns && now >= app->smoke_deadline_ns) app->running = false;
    }
}

static void shutdown(BongoApp *app) {
    if (!app->smoke && app->config_path[0]) {
        BongoError error = {0};
        if (bongo_config_save(app->config_path, &app->config, &error) != BONGO_OK)
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", error.message);
    }
    bongo_preferences_destroy(app->preferences);
    bongo_i18n_destroy(app->i18n);
    bongo_tray_destroy(app->tray);
    bongo_audio_destroy(app->audio);
    bongo_overlay_destroy(app->overlay);
    bongo_live2d_destroy(app->live2d);
    bongo_platform_shutdown(&app->platform);
    bongo_window_destroy(app);
}

int bongo_app_run(int argc, char **argv) {
    if (!bongo_platform_single_instance_begin()) return 0;
    BongoApp app;
    BongoError error = {0};
    if (!initialize(&app, argc, argv, &error)) {
        fprintf(stderr, "%s\n", error.message[0] ? error.message : "Initialization failed");
        shutdown(&app);
        bongo_platform_single_instance_end();
        return 1;
    }
    loop(&app);
    shutdown(&app);
    bool restart = app.restart_requested;
    bongo_platform_single_instance_end();
    if (restart) bongo_platform_restart();
    return app.exit_code;
}
