#include "runtime.h"
#include "bongo/audio.h"
#include "bongo/preferences.h"
#include "bongo/shortcut.h"

#include <stdio.h>
#include <string.h>

static void visible(BongoApp *app) {
    app->config.window.visible = !app->config.window.visible;
    app->config.window.visible ? SDL_ShowWindow(app->window) : SDL_HideWindow(app->window);
    if (app->config.window.visible) app->dirty = true;
}

void bongo_app_shortcuts(BongoApp *app, const BongoInputEvent *event) {
    if (!app || !bongo_shortcut_update(&app->shortcut_state, event)) return;
    BongoShortcutOptions *shortcuts = &app->config.shortcuts;
    if (bongo_shortcut_matches(&app->shortcut_state, event, shortcuts->visible_cat)) {
        visible(app);
    } else if (bongo_shortcut_matches(&app->shortcut_state, event,
        shortcuts->visible_preferences)) {
        bongo_preferences_visible(app->preferences) ?
            bongo_preferences_close(app->preferences) : bongo_preferences_show(app->preferences);
    } else if (bongo_shortcut_matches(&app->shortcut_state, event, shortcuts->mirror)) {
        app->config.model.mirror = !app->config.model.mirror;
        app->dirty = true;
    } else if (bongo_shortcut_matches(&app->shortcut_state, event, shortcuts->pass_through)) {
        app->config.window.pass_through = !app->config.window.pass_through;
        bongo_platform_set_click_through(&app->platform, app->config.window.pass_through);
    } else if (bongo_shortcut_matches(&app->shortcut_state, event,
        shortcuts->always_on_top)) {
        app->config.window.always_on_top = !app->config.window.always_on_top;
        bongo_platform_set_always_on_top(&app->platform, app->config.window.always_on_top);
    } else if (app->config.model.behavior) {
        for (size_t i = 0; i < app->config.behavior_shortcut_count; ++i) {
            BongoBehaviorShortcut *shortcut = &app->config.behavior_shortcuts[i];
            if (!bongo_shortcut_matches(&app->shortcut_state, event, shortcut->shortcut)) continue;
            for (size_t j = 0; j < app->behaviors.count; ++j) {
                BongoBehaviorEntry *behavior = &app->behaviors.entries[j];
                if (strcmp(shortcut->id, behavior->id) != 0) continue;
                if (behavior->kind == BONGO_BEHAVIOR_MOTION) {
                    bool started = bongo_live2d_start_motion(app->live2d,
                        behavior->group, behavior->index);
                    if (started && behavior->sound[0]) {
                        BongoError error = {0};
                        bongo_audio_play(app->audio, behavior->sound, &error);
                    }
                } else bongo_live2d_set_expression(app->live2d, behavior->index);
                app->dirty = true;
                return;
            }
        }
    }
}

static void test_key(BongoApp *app, BongoInputKind kind, const char *name) {
    BongoInputEvent event = {.kind = kind};
    snprintf(event.name, sizeof(event.name), "%s", name);
    bongo_app_shortcuts(app, &event);
}

static void test_press(BongoApp *app, const char *name) {
    test_key(app, BONGO_INPUT_KEY_DOWN, name);
    test_key(app, BONGO_INPUT_KEY_UP, name);
}

bool bongo_app_shortcuts_self_test(BongoApp *app) {
    if (!app || !app->preferences) return false;
    BongoShortcutOptions *keys = &app->config.shortcuts;
    snprintf(keys->visible_cat, sizeof(keys->visible_cat), "Control+B");
    snprintf(keys->visible_preferences, sizeof(keys->visible_preferences), "Control+Comma");
    snprintf(keys->mirror, sizeof(keys->mirror), "Control+M");
    snprintf(keys->pass_through, sizeof(keys->pass_through), "Control+P");
    snprintf(keys->always_on_top, sizeof(keys->always_on_top), "Control+T");
    app->config.window.visible = true;
    app->config.model.mirror = false;
    app->config.window.pass_through = false;
    app->config.window.always_on_top = false;
    test_key(app, BONGO_INPUT_KEY_DOWN, "ControlLeft");
    test_press(app, "KeyB");
    test_press(app, "KeyM");
    test_press(app, "KeyP");
    test_press(app, "KeyT");
    test_press(app, "Comma");
    test_key(app, BONGO_INPUT_KEY_UP, "ControlLeft");
    bool result = !app->config.window.visible && app->config.model.mirror &&
        app->config.window.pass_through && app->config.window.always_on_top &&
        bongo_preferences_visible(app->preferences);
    bongo_preferences_close(app->preferences);
    return result;
}
