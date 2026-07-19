#include "runtime.h"
#include "l2dcat/audio.h"
#include "l2dcat/preferences.h"
#include "l2dcat/shortcut.h"

#include <stdio.h>
#include <string.h>

static void visible(L2DCatApp *app) {
    app->config.window.visible = !app->config.window.visible;
    app->config.window.visible ? SDL_ShowWindow(app->window) : SDL_HideWindow(app->window);
    if (app->config.window.visible) app->dirty = true;
}

static bool run_behavior(L2DCatApp *app, L2DCatBehaviorEntry *behavior) {
    if (behavior->kind == L2DCAT_BEHAVIOR_MOTION) {
        bool started = l2dcat_live2d_start_motion(app->live2d,
            behavior->group, behavior->index);
        if (!started) return false;
        if (behavior->sound[0]) {
            L2DCatError error = {0};
            l2dcat_audio_play(app->audio, behavior->sound, &error);
        }
    } else if (!l2dcat_live2d_set_expression(app->live2d, behavior->index)) return false;
    app->dirty = true;
    return true;
}

static bool behavior_shortcut(L2DCatApp *app, const L2DCatInputEvent *event) {
    for (size_t i = 0; i < app->config.behavior_shortcut_count; ++i) {
        L2DCatBehaviorShortcut *shortcut = &app->config.behavior_shortcuts[i];
        if (!l2dcat_shortcut_matches(&app->shortcut_state, event, shortcut->shortcut)) continue;
        for (size_t j = 0; j < app->behaviors.count; ++j)
            if (strcmp(shortcut->id, app->behaviors.entries[j].id) == 0)
                return run_behavior(app, &app->behaviors.entries[j]);
    }
    size_t limit = app->behaviors.count < 10 ? app->behaviors.count : 10;
    for (size_t i = 0; i < limit; ++i) {
        char alias[8];
        snprintf(alias, sizeof(alias), "Alt+%c", i == 9 ? '0' : (char)('1' + i));
        if (l2dcat_shortcut_matches(&app->shortcut_state, event, alias))
            return run_behavior(app, &app->behaviors.entries[i]);
    }
    return false;
}

void l2dcat_app_shortcuts(L2DCatApp *app, const L2DCatInputEvent *event) {
    if (!app || !l2dcat_shortcut_update(&app->shortcut_state, event)) return;
    L2DCatShortcutOptions *shortcuts = &app->config.shortcuts;
    if (l2dcat_shortcut_matches(&app->shortcut_state, event, shortcuts->visible_cat)) {
        visible(app);
    } else if (l2dcat_shortcut_matches(&app->shortcut_state, event,
        shortcuts->visible_preferences)) {
        l2dcat_preferences_visible(app->preferences) ?
            l2dcat_preferences_close(app->preferences) : l2dcat_preferences_show(app->preferences);
    } else if (l2dcat_shortcut_matches(&app->shortcut_state, event, shortcuts->mirror)) {
        app->config.model.mirror = !app->config.model.mirror;
        app->dirty = true;
        l2dcat_preferences_invalidate(app->preferences);
    } else if (l2dcat_shortcut_matches(&app->shortcut_state, event, shortcuts->pass_through)) {
        app->config.window.pass_through = !app->config.window.pass_through;
        l2dcat_platform_set_click_through(&app->platform, app->config.window.pass_through);
        l2dcat_preferences_invalidate(app->preferences);
    } else if (l2dcat_shortcut_matches(&app->shortcut_state, event,
        shortcuts->always_on_top)) {
        app->config.window.always_on_top = !app->config.window.always_on_top;
        l2dcat_platform_set_always_on_top(&app->platform, app->config.window.always_on_top);
        l2dcat_preferences_invalidate(app->preferences);
    } else if (app->config.model.behavior) behavior_shortcut(app, event);
}

static void test_key(L2DCatApp *app, L2DCatInputKind kind, const char *name) {
    L2DCatInputEvent event = {.kind = kind};
    snprintf(event.name, sizeof(event.name), "%s", name);
    l2dcat_app_shortcuts(app, &event);
}

static void test_press(L2DCatApp *app, const char *name) {
    test_key(app, L2DCAT_INPUT_KEY_DOWN, name);
    test_key(app, L2DCAT_INPUT_KEY_UP, name);
}

bool l2dcat_app_shortcuts_self_test(L2DCatApp *app) {
    if (!app || !app->preferences) return false;
    L2DCatShortcutOptions *keys = &app->config.shortcuts;
    snprintf(keys->visible_cat, sizeof(keys->visible_cat), "Control+B");
    snprintf(keys->visible_preferences, sizeof(keys->visible_preferences), "Control+Comma");
    snprintf(keys->mirror, sizeof(keys->mirror), "Control+M");
    snprintf(keys->pass_through, sizeof(keys->pass_through), "Control+P");
    snprintf(keys->always_on_top, sizeof(keys->always_on_top), "Control+T");
    app->config.window.visible = true;
    app->config.model.mirror = false;
    app->config.window.pass_through = false;
    app->config.window.always_on_top = false;
    test_key(app, L2DCAT_INPUT_KEY_DOWN, "ControlLeft");
    test_press(app, "KeyB");
    test_press(app, "KeyM");
    test_press(app, "KeyP");
    test_press(app, "KeyT");
    test_press(app, "Comma");
    test_key(app, L2DCAT_INPUT_KEY_UP, "ControlLeft");
    bool result = !app->config.window.visible && app->config.model.mirror &&
        app->config.window.pass_through && app->config.window.always_on_top &&
        l2dcat_preferences_visible(app->preferences);
    l2dcat_preferences_close(app->preferences);
    return result;
}
