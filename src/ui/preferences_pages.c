#include "preferences_internal.h"
#include "preferences_widgets.h"
#include "l2dcat/audio.h"
#include "l2dcat/i18n.h"
#include "l2dcat/preferences.h"
#include "l2dcat/tray.h"

#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

static const char *tr(L2DCatApp *app, const char *key, const char *fallback) {
    return l2dcat_i18n_get(app->i18n, key, fallback);
}

static void restore_hover(L2DCatApp *app) {
    if (!app->hover_hidden) return;
    SDL_SetWindowOpacity(app->window, app->config.window.opacity_percent / 100.0f);
    l2dcat_platform_set_click_through(&app->platform, app->config.window.pass_through);
    app->hover_hidden = false;
}

void l2dcat_preferences_page_cat(L2DCatApp *app, struct nk_context *context) {
    L2DCatModelOptions *model = &app->config.model;
    L2DCatWindowOptions *window = &app->config.window;
    l2dcat_pref_section(context, tr(app, "pages.preference.cat.labels.modelSettings",
        "Model Settings"));
    if (l2dcat_pref_toggle(context, "mirror", tr(app,
        "pages.preference.cat.labels.mirrorMode", "Mirror Mode"), tr(app,
        "pages.preference.cat.hints.mirrorMode", "Mirror the model horizontally."),
        &model->mirror)) app->dirty = true;
    l2dcat_pref_toggle(context, "mouse-mirror", tr(app,
        "pages.preference.cat.labels.mouseMirror", "Mouse Mirror"), tr(app,
        "pages.preference.cat.hints.mouseMirror", "Mirror mouse-driven movement."),
        &model->mouse_mirror);
    l2dcat_pref_toggle(context, "ignore-mouse", tr(app,
        "pages.preference.cat.labels.ignoreMouse", "Ignore Mouse Events"), tr(app,
        "pages.preference.cat.hints.ignoreMouse", "Do not react to mouse movement."),
        &model->ignore_mouse);
    if (l2dcat_pref_toggle(context, "motion-sound", tr(app,
        "pages.preference.cat.labels.motionSound", "Motion Sound"), tr(app,
        "pages.preference.cat.hints.motionSound", "Play sounds attached to motions."),
        &model->motion_sound)) l2dcat_audio_set_enabled(app->audio, model->motion_sound);
    l2dcat_pref_toggle(context, "behavior", tr(app,
        "pages.preference.cat.labels.behavior", "Motions and Expressions"), tr(app,
        "pages.preference.cat.hints.behavior", "Configure motion and expression shortcuts."),
        &model->behavior);
#ifdef _WIN32
    l2dcat_pref_float(context, "release-delay", tr(app,
        "pages.preference.cat.labels.autoReleaseDelay", "Auto Release Delay"), tr(app,
        "pages.preference.cat.hints.autoReleaseDelay", "Release system keys after timeout."),
        .05f, &model->auto_release_seconds, 30.0f, .05f);
#endif
    l2dcat_pref_int(context, "max-fps", tr(app,
        "pages.preference.cat.labels.maxFPS", "Max Frame Rate"), tr(app,
        "pages.preference.cat.hints.maxFPS", "Lower values reduce resource usage."),
        1, &model->max_fps, 240, 1);

    l2dcat_pref_section(context, tr(app, "pages.preference.cat.labels.windowSettings",
        "Window Settings"));
    if (l2dcat_pref_toggle(context, "pass-through", tr(app,
        "pages.preference.cat.labels.passThrough", "Pass Through"), tr(app,
        "pages.preference.cat.hints.passThrough", "Allow clicks to pass through the window."),
        &window->pass_through))
        l2dcat_platform_set_click_through(&app->platform, window->pass_through);
    if (l2dcat_pref_toggle(context, "always-top", tr(app,
        "pages.preference.cat.labels.alwaysOnTop", "Always on Top"), tr(app,
        "pages.preference.cat.hints.alwaysOnTop", "Keep the cat above other windows."),
        &window->always_on_top))
        l2dcat_platform_set_always_on_top(&app->platform, window->always_on_top);
    if (l2dcat_pref_toggle(context, "hide-hover", tr(app,
        "pages.preference.cat.labels.hideOnHover", "Hide on Hover"), tr(app,
        "pages.preference.cat.hints.hideOnHover", "Hide when the pointer enters the window."),
        &window->hide_on_hover) && !window->hide_on_hover) restore_hover(app);
    if (window->hide_on_hover)
        l2dcat_pref_float(context, "hover-delay", tr(app, "native.hoverDelay",
            "Hover delay (seconds)"), "", 0.0f, &window->hide_delay_seconds, 60.0f, .1f);
    l2dcat_pref_toggle(context, "keep-screen", tr(app,
        "pages.preference.cat.labels.keepInScreen", "Keep on Screen"), tr(app,
        "pages.preference.cat.hints.keepInScreen", "Keep the window inside its monitor."),
        &window->keep_in_screen);
    float old_scale = window->scale_percent;
    l2dcat_pref_float(context, "window-size", tr(app,
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
    l2dcat_pref_float(context, "radius", tr(app,
        "pages.preference.cat.labels.windowRadius", "Window Radius"), "",
        0.0f, &window->radius_percent, 50.0f, 1.0f);
    if (old_radius != window->radius_percent) app->dirty = true;
    float old_opacity = window->opacity_percent;
    l2dcat_pref_slider(context, "opacity", tr(app,
        "pages.preference.cat.labels.opacity", "Opacity"), "",
        10.0f, &window->opacity_percent, 100.0f, 1.0f);
    if (old_opacity != window->opacity_percent && !app->hover_hidden)
        SDL_SetWindowOpacity(app->window, window->opacity_percent / 100.0f);
}

static void update_autostart(L2DCatApp *app, bool old_value) {
    L2DCatError error = {0};
    if (l2dcat_platform_set_autostart(app->config.app.autostart, &error) == L2DCAT_OK) return;
    app->config.app.autostart = old_value;
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", error.message);
}

static void update_tray(L2DCatApp *app) {
    if (!app->config.app.tray_visible) {
        l2dcat_tray_destroy(app->tray); app->tray = NULL; return;
    }
    if (app->tray) return;
    L2DCatError error = {0}; app->tray = l2dcat_tray_create(app, &error);
    if (!app->tray) {
        app->config.app.tray_visible = false;
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", error.message);
    }
}

void l2dcat_preferences_page_general(L2DCatApp *app, struct nk_context *context) {
    L2DCatAppOptions *options = &app->config.app;
    l2dcat_pref_section(context, tr(app, "pages.preference.general.labels.appSettings",
        "Application Settings"));
    bool old_autostart = options->autostart;
    if (l2dcat_pref_toggle(context, "autostart", tr(app,
        "pages.preference.general.labels.launchOnStartup", "Launch on Startup"), "",
        &options->autostart)) update_autostart(app, old_autostart);
    if (l2dcat_pref_toggle(context, "taskbar", tr(app,
        "pages.preference.general.labels.showTaskbarIcon", "Show Taskbar Icon"), tr(app,
        "pages.preference.general.hints.showTaskbarIcon", "Allows window capture in OBS."),
        &app->config.window.taskbar_visible))
        l2dcat_platform_set_taskbar(&app->platform, app->config.window.taskbar_visible);
    if (l2dcat_pref_toggle(context, "tray", tr(app,
        "pages.preference.general.labels.showTrayIcon", "Show Tray Icon"), tr(app,
        "pages.preference.general.hints.showTrayIcon", "Show l2dcat in the system tray."),
        &options->tray_visible)) update_tray(app);

    l2dcat_pref_section(context, tr(app,
        "pages.preference.general.labels.appearanceSettings", "Appearance Settings"));
    const char *themes[] = {tr(app, "pages.preference.general.options.auto", "System"),
        tr(app, "pages.preference.general.options.lightMode", "Light"),
        tr(app, "pages.preference.general.options.darkMode", "Dark")};
    const char *languages[] = {"English", "简体中文", "繁體中文", "Português", "Tiếng Việt"};
    options->theme = (L2DCatTheme)l2dcat_pref_combo(context, "theme", tr(app,
        "pages.preference.general.labels.themeMode", "Theme Mode"), "",
        themes, 3, options->theme);
    options->language = (L2DCatLanguage)l2dcat_pref_combo(context, "language", tr(app,
        "pages.preference.general.labels.language", "Language"), "",
        languages, 5, options->language);

    l2dcat_pref_section(context, tr(app, "native.platformStatus", "Platform status"));
    l2dcat_pref_status(context, "global-input", l2dcat_platform_global_input_supported()
        ? tr(app, "native.globalInputAvailable", "Global input: available")
        : tr(app, "native.globalInputReduced", "Global input: reduced support"), "");
    l2dcat_pref_status(context, "privileges", l2dcat_platform_is_elevated()
        ? tr(app, "native.privilegesAdmin", "Process privileges: administrator")
        : tr(app, "native.privilegesStandard", "Process privileges: standard user"), "");
}

void l2dcat_preferences_page_shortcuts(L2DCatApp *app, struct nk_context *context) {
    L2DCatShortcutOptions *value = &app->config.shortcuts;
    l2dcat_pref_section(context, tr(app, "pages.preference.shortcut.title", "Shortcuts"));
    l2dcat_pref_edit(context, "shortcut-cat", tr(app,
        "pages.preference.shortcut.labels.toggleCat", "Toggle Cat"), tr(app,
        "pages.preference.shortcut.hints.toggleCat", "Toggle the cat window."),
        value->visible_cat, sizeof(value->visible_cat));
    l2dcat_pref_edit(context, "shortcut-pref", tr(app,
        "pages.preference.shortcut.labels.togglePreferences", "Toggle Preferences"), tr(app,
        "pages.preference.shortcut.hints.togglePreferences", "Toggle this window."),
        value->visible_preferences, sizeof(value->visible_preferences));
    l2dcat_pref_edit(context, "shortcut-mirror", tr(app,
        "pages.preference.shortcut.labels.mirrorMode", "Mirror Mode"), tr(app,
        "pages.preference.shortcut.hints.mirrorMode", "Toggle horizontal mirroring."),
        value->mirror, sizeof(value->mirror));
    l2dcat_pref_edit(context, "shortcut-pass", tr(app,
        "pages.preference.shortcut.labels.passThrough", "Pass Through"), tr(app,
        "pages.preference.shortcut.hints.passThrough", "Toggle mouse pass-through."),
        value->pass_through, sizeof(value->pass_through));
    l2dcat_pref_edit(context, "shortcut-top", tr(app,
        "pages.preference.shortcut.labels.alwaysOnTop", "Always on Top"), tr(app,
        "pages.preference.shortcut.hints.alwaysOnTop", "Toggle always-on-top."),
        value->always_on_top, sizeof(value->always_on_top));
}

void l2dcat_preferences_page_about(L2DCatApp *app, struct nk_context *context) {
    l2dcat_pref_section(context, tr(app,
        "pages.preference.about.labels.aboutApp", "About App"));
    char version[64]; snprintf(version, sizeof(version), "l2dcat %s", L2DCAT_VERSION);
    l2dcat_pref_status(context, "about-version", version, tr(app,
        "native.aboutDescription", "Standalone native application."));
#ifdef L2DCAT_HAS_CUBISM
    l2dcat_pref_status(context, "renderer", tr(app, "native.rendererCubism",
        "Live2D renderer: Cubism Native"), "");
#else
    l2dcat_pref_status(context, "renderer", tr(app, "native.rendererDiagnostic",
        "Live2D renderer: diagnostic mode"), "");
#endif
    if (l2dcat_pref_button(context, "copy-info", tr(app,
        "pages.preference.about.labels.appInfo", "App Info"), tr(app,
        "pages.preference.about.hints.appInfo", "Copy application information."), tr(app,
        "pages.preference.about.buttons.copy", "Copy"))) {
        char info[256];
        snprintf(info, sizeof(info), "l2dcat %s\n%s: %s\n%s: %s",
            L2DCAT_VERSION, tr(app, "native.platformLabel", "Platform"), SDL_GetPlatform(),
            tr(app, "native.globalInputLabel", "Global input"),
            l2dcat_platform_global_input_supported() ? tr(app, "native.available", "available")
                : tr(app, "native.reduced", "reduced"));
        SDL_SetClipboardText(info);
    }
}
