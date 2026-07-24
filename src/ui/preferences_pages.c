#include "preferences_internal.h"
#include "preferences_widgets.h"
#include "preferences_notice.h"
#include "bongo_cat_neo/audio.h"
#include "bongo_cat_neo/i18n.h"
#include "bongo_cat_neo/preferences.h"
#include "bongo_cat_neo/tray.h"
#include "../runtime/runtime.h"

#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

static const char *tr(BongoCatNeoApp *app, const char *key, const char *fallback) {
    return bongo_cat_neo_i18n_get(app->i18n, key, fallback);
}

static void restore_hover(BongoCatNeoApp *app) {
    if (!app->hover_hidden) return;
    SDL_SetWindowOpacity(app->window, app->config.window.opacity_percent / 100.0f);
    app->hover_hidden = false;
    bongo_cat_neo_window_sync_click_through(app);
}

void bongo_cat_neo_preferences_page_cat(BongoCatNeoApp *app, struct nk_context *context) {
    BongoCatNeoModelOptions *model = &app->config.model;
    BongoCatNeoWindowOptions *window = &app->config.window;
    bongo_cat_neo_pref_section(context, tr(app, "pages.preference.cat.labels.windowSettings",
        "Window Settings"));
    if (bongo_cat_neo_pref_toggle(context, "pass-through", tr(app,
        "pages.preference.cat.labels.passThrough", "Pass Through"), tr(app,
        "pages.preference.cat.hints.passThrough", "Allow clicks to pass through the window."),
        &window->pass_through)) {
        bongo_cat_neo_window_mark_hit_dirty(app);
        bongo_cat_neo_window_sync_click_through(app);
    }
    if (bongo_cat_neo_pref_toggle(context, "always-top", tr(app,
        "pages.preference.cat.labels.alwaysOnTop", "Always on Top"), tr(app,
        "pages.preference.cat.hints.alwaysOnTop", "Keep the cat above other windows."),
        &window->always_on_top))
        bongo_cat_neo_platform_set_always_on_top(&app->platform, window->always_on_top);
    if (bongo_cat_neo_pref_toggle(context, "hide-hover", tr(app,
        "pages.preference.cat.labels.hideOnHover", "Hide on Hover"), tr(app,
        "pages.preference.cat.hints.hideOnHover", "Hide when the pointer enters the window."),
        &window->hide_on_hover) && !window->hide_on_hover) restore_hover(app);
    if (window->hide_on_hover)
        bongo_cat_neo_pref_float(context, "hover-delay", tr(app, "native.hoverDelay",
            "Hover delay (seconds)"), "", 0.0f, &window->hide_delay_seconds, 60.0f, .1f);
    bongo_cat_neo_pref_toggle(context, "keep-screen", tr(app,
        "pages.preference.cat.labels.keepInScreen", "Keep on Screen"), tr(app,
        "pages.preference.cat.hints.keepInScreen", "Keep the window inside its monitor."),
        &window->keep_in_screen);
    float old_scale = window->scale_percent;
    bongo_cat_neo_pref_float(context, "window-size", tr(app,
        "pages.preference.cat.labels.windowSize", "Window Size"), tr(app,
        "pages.preference.cat.hints.windowSize", "Resize with the edge or Shift + right drag."),
        10.0f, &window->scale_percent, 500.0f, 1.0f);
    if (old_scale != window->scale_percent && old_scale > 0.0f) {
        float requested_scale = window->scale_percent;
        window->scale_percent = old_scale;
        bongo_cat_neo_window_cancel_wheel_animation(app);
        bongo_cat_neo_window_set_scale(app, requested_scale);
    }
    float old_opacity = window->opacity_percent;
    bongo_cat_neo_pref_slider(context, "opacity", tr(app,
        "pages.preference.cat.labels.opacity", "Opacity"), "",
        10.0f, &window->opacity_percent, 100.0f, 1.0f);
    if (old_opacity != window->opacity_percent) bongo_cat_neo_window_cancel_wheel_animation(app);
    if (old_opacity != window->opacity_percent && !app->hover_hidden)
        SDL_SetWindowOpacity(app->window, window->opacity_percent / 100.0f);

    bongo_cat_neo_pref_section(context, tr(app, "pages.preference.cat.labels.modelSettings",
        "Model Settings"));
    if (bongo_cat_neo_pref_toggle(context, "mirror", tr(app,
        "pages.preference.cat.labels.mirrorMode", "Mirror Mode"), "",
        &model->mirror)) app->dirty = true;
    bongo_cat_neo_pref_toggle(context, "mouse-mirror", tr(app,
        "pages.preference.cat.labels.mouseMirror", "Mouse Mirror"), "",
        &model->mouse_mirror);
    bongo_cat_neo_pref_toggle(context, "ignore-mouse", tr(app,
        "pages.preference.cat.labels.ignoreMouse", "Ignore Mouse Events"), "",
        &model->ignore_mouse);
    if (bongo_cat_neo_pref_toggle(context, "motion-sound", tr(app,
        "pages.preference.cat.labels.motionSound", "Motion Sound"), tr(app,
        "pages.preference.cat.hints.motionSound", "Play sounds attached to motions."),
        &model->motion_sound)) bongo_cat_neo_audio_set_enabled(app->audio, model->motion_sound);
    bongo_cat_neo_pref_toggle(context, "behavior", tr(app,
        "pages.preference.cat.labels.behavior", "Motions and Expressions"), tr(app,
        "pages.preference.cat.hints.behavior", "Configure motion and expression shortcuts."),
        &model->behavior);
#ifdef _WIN32
    bongo_cat_neo_pref_float(context, "release-delay", tr(app,
        "pages.preference.cat.labels.autoReleaseDelay", "Auto Release Delay"), tr(app,
        "pages.preference.cat.hints.autoReleaseDelay", "Release system keys after timeout."),
        .05f, &model->auto_release_seconds, 30.0f, .05f);
#endif
    bongo_cat_neo_pref_int(context, "max-fps", tr(app,
        "pages.preference.cat.labels.maxFPS", "Max Frame Rate"), tr(app,
        "pages.preference.cat.hints.maxFPS", "Lower values reduce resource usage."),
        1, &model->max_fps, 240, 1);
}

static void update_autostart(BongoCatNeoApp *app, bool old_value) {
    BongoCatNeoError error = {0};
    if (bongo_cat_neo_platform_set_autostart(app->config.app.autostart, &error) == BONGO_CAT_NEO_OK) return;
    app->config.app.autostart = old_value;
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", error.message);
    bongo_cat_neo_preferences_notice_show(app, error.message, true);
}

static void update_tray(BongoCatNeoApp *app) {
    if (!app->config.app.tray_visible) {
        bongo_cat_neo_tray_destroy(app->tray); app->tray = NULL; return;
    }
    if (app->tray) return;
    BongoCatNeoError error = {0}; app->tray = bongo_cat_neo_tray_create(app, &error);
    if (!app->tray) {
        app->config.app.tray_visible = false;
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", error.message);
        bongo_cat_neo_preferences_notice_show(app, error.message, true);
    }
}

void bongo_cat_neo_preferences_page_general(BongoCatNeoApp *app, struct nk_context *context) {
    BongoCatNeoAppOptions *options = &app->config.app;
    const char *themes[] = {tr(app, "pages.preference.general.options.auto", "System"),
        tr(app, "pages.preference.general.options.lightMode", "Light"),
        tr(app, "pages.preference.general.options.darkMode", "Dark")};
    const char *languages[] = {"English", "简体中文", "繁體中文", "Português", "Tiếng Việt"};
    bongo_cat_neo_pref_section(context, tr(app,
        "pages.preference.general.labels.appearanceSettings", "Appearance Settings"));
    options->theme = (BongoCatNeoTheme)bongo_cat_neo_pref_combo(context, "theme", tr(app,
        "pages.preference.general.labels.themeMode", "Theme Mode"), "",
        themes, 3, options->theme);
    options->language = (BongoCatNeoLanguage)bongo_cat_neo_pref_combo(context, "language", tr(app,
        "pages.preference.general.labels.language", "Language"), "",
        languages, 5, options->language);

    bongo_cat_neo_pref_section(context, tr(app, "pages.preference.general.labels.appSettings",
        "Application Settings"));
    bool old_autostart = options->autostart;
    if (bongo_cat_neo_pref_toggle(context, "autostart", tr(app,
        "pages.preference.general.labels.launchOnStartup", "Launch on Startup"), "",
        &options->autostart)) update_autostart(app, old_autostart);
    if (bongo_cat_neo_pref_toggle(context, "taskbar", tr(app,
        "pages.preference.general.labels.showTaskbarIcon", "Show Taskbar Icon"), tr(app,
        "pages.preference.general.hints.showTaskbarIcon", "Allows window capture in OBS."),
        &app->config.window.taskbar_visible)) {
        bongo_cat_neo_platform_set_taskbar(&app->platform, app->config.window.taskbar_visible);
        app->dirty = true;
    }
    if (bongo_cat_neo_pref_toggle(context, "tray", tr(app,
        "pages.preference.general.labels.showTrayIcon", "Show Tray Icon"), tr(app,
        "pages.preference.general.hints.showTrayIcon", "Show Bongo Cat Neo in the system tray."),
        &options->tray_visible)) update_tray(app);

}

void bongo_cat_neo_preferences_page_about(BongoCatNeoApp *app, struct nk_context *context) {
    bongo_cat_neo_pref_section(context, tr(app,
        "pages.preference.about.labels.aboutApp", "About App"));
    char version[64]; snprintf(version, sizeof(version), "%s %s",
        BONGO_CAT_NEO_NAME, BONGO_CAT_NEO_VERSION);
    bongo_cat_neo_pref_status(context, "about-version", version, "");
}
