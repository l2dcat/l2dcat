#include "preferences_internal.h"
#include "preferences_widgets.h"
#include "bongo/audio.h"
#include "bongo/i18n.h"
#include "bongo/preferences.h"
#include "bongo/tray.h"

#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

static const char *tr(BongoApp *app, const char *key, const char *fallback) {
    return bongo_i18n_get(app->i18n, key, fallback);
}

static void restore_hover(BongoApp *app) {
    if (!app->hover_hidden) return;
    SDL_SetWindowOpacity(app->window, app->config.window.opacity_percent / 100.0f);
    bongo_platform_set_click_through(&app->platform, app->config.window.pass_through);
    app->hover_hidden = false;
}

void bongo_preferences_page_cat(BongoApp *app, struct nk_context *context) {
    BongoModelOptions *model = &app->config.model;
    BongoWindowOptions *window = &app->config.window;
    bongo_pref_section(context, tr(app, "pages.preference.cat.labels.modelSettings",
        "Model Settings"));
    if (bongo_pref_toggle(context, "mirror", tr(app,
        "pages.preference.cat.labels.mirrorMode", "Mirror Mode"), tr(app,
        "pages.preference.cat.hints.mirrorMode", "Mirror the model horizontally."),
        &model->mirror)) app->dirty = true;
    bongo_pref_toggle(context, "mouse-mirror", tr(app,
        "pages.preference.cat.labels.mouseMirror", "Mouse Mirror"), tr(app,
        "pages.preference.cat.hints.mouseMirror", "Mirror mouse-driven movement."),
        &model->mouse_mirror);
    bongo_pref_toggle(context, "ignore-mouse", tr(app,
        "pages.preference.cat.labels.ignoreMouse", "Ignore Mouse Events"), tr(app,
        "pages.preference.cat.hints.ignoreMouse", "Do not react to mouse movement."),
        &model->ignore_mouse);
    if (bongo_pref_toggle(context, "motion-sound", tr(app,
        "pages.preference.cat.labels.motionSound", "Motion Sound"), tr(app,
        "pages.preference.cat.hints.motionSound", "Play sounds attached to motions."),
        &model->motion_sound)) bongo_audio_set_enabled(app->audio, model->motion_sound);
    bongo_pref_toggle(context, "behavior", tr(app,
        "pages.preference.cat.labels.behavior", "Motions and Expressions"), tr(app,
        "pages.preference.cat.hints.behavior", "Configure motion and expression shortcuts."),
        &model->behavior);
#ifdef _WIN32
    bongo_pref_float(context, "release-delay", tr(app,
        "pages.preference.cat.labels.autoReleaseDelay", "Auto Release Delay"), tr(app,
        "pages.preference.cat.hints.autoReleaseDelay", "Release system keys after timeout."),
        .05f, &model->auto_release_seconds, 30.0f, .05f);
#endif
    bongo_pref_int(context, "max-fps", tr(app,
        "pages.preference.cat.labels.maxFPS", "Max Frame Rate"), tr(app,
        "pages.preference.cat.hints.maxFPS", "Lower values reduce resource usage."),
        1, &model->max_fps, 240, 1);

    bongo_pref_section(context, tr(app, "pages.preference.cat.labels.windowSettings",
        "Window Settings"));
    if (bongo_pref_toggle(context, "pass-through", tr(app,
        "pages.preference.cat.labels.passThrough", "Pass Through"), tr(app,
        "pages.preference.cat.hints.passThrough", "Allow clicks to pass through the window."),
        &window->pass_through))
        bongo_platform_set_click_through(&app->platform, window->pass_through);
    if (bongo_pref_toggle(context, "always-top", tr(app,
        "pages.preference.cat.labels.alwaysOnTop", "Always on Top"), tr(app,
        "pages.preference.cat.hints.alwaysOnTop", "Keep the cat above other windows."),
        &window->always_on_top))
        bongo_platform_set_always_on_top(&app->platform, window->always_on_top);
    if (bongo_pref_toggle(context, "hide-hover", tr(app,
        "pages.preference.cat.labels.hideOnHover", "Hide on Hover"), tr(app,
        "pages.preference.cat.hints.hideOnHover", "Hide when the pointer enters the window."),
        &window->hide_on_hover) && !window->hide_on_hover) restore_hover(app);
    if (window->hide_on_hover)
        bongo_pref_float(context, "hover-delay", tr(app, "native.hoverDelay",
            "Hover delay (seconds)"), "", 0.0f, &window->hide_delay_seconds, 60.0f, .1f);
    bongo_pref_toggle(context, "keep-screen", tr(app,
        "pages.preference.cat.labels.keepInScreen", "Keep on Screen"), tr(app,
        "pages.preference.cat.hints.keepInScreen", "Keep the window inside its monitor."),
        &window->keep_in_screen);
    float old_scale = window->scale_percent;
    bongo_pref_float(context, "window-size", tr(app,
        "pages.preference.cat.labels.windowSize", "Window Size"), tr(app,
        "pages.preference.cat.hints.windowSize", "Resize with the edge or Shift + right drag."),
        10.0f, &window->scale_percent, 500.0f, 1.0f);
    if (old_scale != window->scale_percent && old_scale > 0.0f) {
        float factor = window->scale_percent / old_scale;
        window->width = (int)(window->width * factor);
        window->height = (int)(window->height * factor);
        SDL_SetWindowSize(app->window, window->width, window->height);
        app->dirty = true;
    }
    float old_radius = window->radius_percent;
    bongo_pref_float(context, "radius", tr(app,
        "pages.preference.cat.labels.windowRadius", "Window Radius"), "",
        0.0f, &window->radius_percent, 50.0f, 1.0f);
    if (old_radius != window->radius_percent) app->dirty = true;
    float old_opacity = window->opacity_percent;
    bongo_pref_slider(context, "opacity", tr(app,
        "pages.preference.cat.labels.opacity", "Opacity"), "",
        10.0f, &window->opacity_percent, 100.0f, 1.0f);
    if (old_opacity != window->opacity_percent && !app->hover_hidden)
        SDL_SetWindowOpacity(app->window, window->opacity_percent / 100.0f);
}

static void update_autostart(BongoApp *app, bool old_value) {
    BongoError error = {0};
    if (bongo_platform_set_autostart(app->config.app.autostart, &error) == BONGO_OK) return;
    app->config.app.autostart = old_value;
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", error.message);
}

static void update_tray(BongoApp *app) {
    if (!app->config.app.tray_visible) {
        bongo_tray_destroy(app->tray); app->tray = NULL; return;
    }
    if (app->tray) return;
    BongoError error = {0}; app->tray = bongo_tray_create(app, &error);
    if (!app->tray) {
        app->config.app.tray_visible = false;
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", error.message);
    }
}

void bongo_preferences_page_general(BongoApp *app, struct nk_context *context) {
    BongoAppOptions *options = &app->config.app;
    bongo_pref_section(context, tr(app, "pages.preference.general.labels.appSettings",
        "Application Settings"));
    bool old_autostart = options->autostart;
    if (bongo_pref_toggle(context, "autostart", tr(app,
        "pages.preference.general.labels.launchOnStartup", "Launch on Startup"), "",
        &options->autostart)) update_autostart(app, old_autostart);
    if (bongo_pref_toggle(context, "taskbar", tr(app,
        "pages.preference.general.labels.showTaskbarIcon", "Show Taskbar Icon"), tr(app,
        "pages.preference.general.hints.showTaskbarIcon", "Allows window capture in OBS."),
        &app->config.window.taskbar_visible))
        bongo_platform_set_taskbar(&app->platform, app->config.window.taskbar_visible);
    if (bongo_pref_toggle(context, "tray", tr(app,
        "pages.preference.general.labels.showTrayIcon", "Show Tray Icon"), tr(app,
        "pages.preference.general.hints.showTrayIcon", "Show BongoCat in the system tray."),
        &options->tray_visible)) update_tray(app);

    bongo_pref_section(context, tr(app,
        "pages.preference.general.labels.appearanceSettings", "Appearance Settings"));
    const char *themes[] = {tr(app, "pages.preference.general.options.auto", "System"),
        tr(app, "pages.preference.general.options.lightMode", "Light"),
        tr(app, "pages.preference.general.options.darkMode", "Dark")};
    const char *languages[] = {"English", "简体中文", "繁體中文", "Português", "Tiếng Việt"};
    options->theme = (BongoTheme)bongo_pref_combo(context, "theme", tr(app,
        "pages.preference.general.labels.themeMode", "Theme Mode"), "",
        themes, 3, options->theme);
    options->language = (BongoLanguage)bongo_pref_combo(context, "language", tr(app,
        "pages.preference.general.labels.language", "Language"), "",
        languages, 5, options->language);

    bongo_pref_section(context, tr(app, "native.platformStatus", "Platform status"));
    bongo_pref_status(context, "global-input", bongo_platform_global_input_supported()
        ? tr(app, "native.globalInputAvailable", "Global input: available")
        : tr(app, "native.globalInputReduced", "Global input: reduced support"), "");
    bongo_pref_status(context, "privileges", bongo_platform_is_elevated()
        ? tr(app, "native.privilegesAdmin", "Process privileges: administrator")
        : tr(app, "native.privilegesStandard", "Process privileges: standard user"), "");
}

void bongo_preferences_page_shortcuts(BongoApp *app, struct nk_context *context) {
    BongoShortcutOptions *value = &app->config.shortcuts;
    bongo_pref_section(context, tr(app, "pages.preference.shortcut.title", "Shortcuts"));
    bongo_pref_edit(context, "shortcut-cat", tr(app,
        "pages.preference.shortcut.labels.toggleCat", "Toggle Cat"), tr(app,
        "pages.preference.shortcut.hints.toggleCat", "Toggle the cat window."),
        value->visible_cat, sizeof(value->visible_cat));
    bongo_pref_edit(context, "shortcut-pref", tr(app,
        "pages.preference.shortcut.labels.togglePreferences", "Toggle Preferences"), tr(app,
        "pages.preference.shortcut.hints.togglePreferences", "Toggle this window."),
        value->visible_preferences, sizeof(value->visible_preferences));
    bongo_pref_edit(context, "shortcut-mirror", tr(app,
        "pages.preference.shortcut.labels.mirrorMode", "Mirror Mode"), tr(app,
        "pages.preference.shortcut.hints.mirrorMode", "Toggle horizontal mirroring."),
        value->mirror, sizeof(value->mirror));
    bongo_pref_edit(context, "shortcut-pass", tr(app,
        "pages.preference.shortcut.labels.passThrough", "Pass Through"), tr(app,
        "pages.preference.shortcut.hints.passThrough", "Toggle mouse pass-through."),
        value->pass_through, sizeof(value->pass_through));
    bongo_pref_edit(context, "shortcut-top", tr(app,
        "pages.preference.shortcut.labels.alwaysOnTop", "Always on Top"), tr(app,
        "pages.preference.shortcut.hints.alwaysOnTop", "Toggle always-on-top."),
        value->always_on_top, sizeof(value->always_on_top));
}

void bongo_preferences_page_about(BongoApp *app, struct nk_context *context) {
    bongo_pref_section(context, tr(app,
        "pages.preference.about.labels.aboutApp", "About App"));
    char version[64]; snprintf(version, sizeof(version), "BongoCat %s", BONGO_VERSION);
    bongo_pref_status(context, "about-version", version, tr(app,
        "native.aboutDescription", "Standalone native application."));
#ifdef BONGO_HAS_CUBISM
    bongo_pref_status(context, "renderer", tr(app, "native.rendererCubism",
        "Live2D renderer: Cubism Native"), "");
#else
    bongo_pref_status(context, "renderer", tr(app, "native.rendererDiagnostic",
        "Live2D renderer: diagnostic mode"), "");
#endif
    if (bongo_pref_button(context, "copy-info", tr(app,
        "pages.preference.about.labels.appInfo", "App Info"), tr(app,
        "pages.preference.about.hints.appInfo", "Copy application information."), tr(app,
        "pages.preference.about.buttons.copy", "Copy"))) {
        char info[256];
        snprintf(info, sizeof(info), "BongoCat %s\n%s: %s\n%s: %s",
            BONGO_VERSION, tr(app, "native.platformLabel", "Platform"), SDL_GetPlatform(),
            tr(app, "native.globalInputLabel", "Global input"),
            bongo_platform_global_input_supported() ? tr(app, "native.available", "available")
                : tr(app, "native.reduced", "reduced"));
        SDL_SetClipboardText(info);
    }
}
