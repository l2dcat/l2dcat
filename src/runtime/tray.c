#include "bongo/tray.h"
#include "bongo/app.h"
#include "bongo/image.h"
#include "bongo/i18n.h"
#include "bongo/path.h"
#include "bongo/preferences.h"

#include <SDL3/SDL.h>
#include <stdlib.h>

struct BongoTray {
    BongoApp *app;
    SDL_Tray *handle;
    SDL_TrayEntry *visible;
    SDL_TrayEntry *pass_through;
    SDL_TrayEntry *always_on_top;
    SDL_TrayEntry *preferences;
    SDL_TrayEntry *exit;
    BongoImage icon;
};

static void on_visible(void *userdata, SDL_TrayEntry *entry) {
    (void)entry;
    BongoTray *tray = userdata;
    BongoApp *app = tray->app;
    app->config.window.visible = !app->config.window.visible;
    app->config.window.visible ? SDL_ShowWindow(app->window) : SDL_HideWindow(app->window);
    bongo_tray_sync(tray);
}

static void on_pass_through(void *userdata, SDL_TrayEntry *entry) {
    (void)entry;
    BongoTray *tray = userdata;
    tray->app->config.window.pass_through = !tray->app->config.window.pass_through;
    bongo_platform_set_click_through(&tray->app->platform,
        tray->app->config.window.pass_through);
    bongo_tray_sync(tray);
}

static void on_always_on_top(void *userdata, SDL_TrayEntry *entry) {
    (void)entry;
    BongoTray *tray = userdata;
    tray->app->config.window.always_on_top = !tray->app->config.window.always_on_top;
    bongo_platform_set_always_on_top(&tray->app->platform,
        tray->app->config.window.always_on_top);
    bongo_tray_sync(tray);
}

static void on_preferences(void *userdata, SDL_TrayEntry *entry) {
    (void)entry;
    BongoTray *tray = userdata;
    bongo_preferences_show(tray->app->preferences);
}

static void on_exit(void *userdata, SDL_TrayEntry *entry) {
    (void)entry;
    ((BongoTray *)userdata)->app->running = false;
}

static SDL_TrayEntry *add(SDL_TrayMenu *menu, const char *label,
    SDL_TrayEntryFlags flags, SDL_TrayCallback callback, BongoTray *tray) {
    SDL_TrayEntry *entry = SDL_InsertTrayEntryAt(menu, -1, label, flags);
    if (entry && callback) SDL_SetTrayEntryCallback(entry, callback, tray);
    return entry;
}

BongoTray *bongo_tray_create(BongoApp *app, BongoError *error) {
    if (!app || !app->config.app.tray_visible) return NULL;
    BongoTray *tray = calloc(1, sizeof(*tray));
    if (!tray) return NULL;
    tray->app = app;
    char path[BONGO_PATH_CAP];
    bongo_path_join(path, sizeof(path), app->asset_root,
#ifdef __APPLE__
        "tray-mac.png");
#else
        "tray.png");
#endif
    if (bongo_image_load(path, &tray->icon, error) != BONGO_OK) {
        bongo_tray_destroy(tray);
        return NULL;
    }
    tray->handle = SDL_CreateTray(tray->icon.surface, BONGO_NAME);
    bongo_image_free(&tray->icon);
    if (!tray->handle) {
        bongo_error_set(error, BONGO_ERROR_PLATFORM, "Tray creation failed: %s", SDL_GetError());
        bongo_tray_destroy(tray);
        return NULL;
    }
    SDL_TrayMenu *menu = SDL_CreateTrayMenu(tray->handle);
    tray->visible = add(menu, bongo_i18n_get(app->i18n,
        "composables.useAppMenu.labels.showCat", "Show BongoCat"), SDL_TRAYENTRY_CHECKBOX,
        on_visible, tray);
    tray->pass_through = add(menu, bongo_i18n_get(app->i18n,
        "composables.useAppMenu.labels.passThrough", "Mouse pass-through"), SDL_TRAYENTRY_CHECKBOX,
        on_pass_through, tray);
    tray->always_on_top = add(menu, bongo_i18n_get(app->i18n,
        "pages.preference.cat.labels.alwaysOnTop", "Always on top"), SDL_TRAYENTRY_CHECKBOX,
        on_always_on_top, tray);
    add(menu, NULL, 0, NULL, tray);
    tray->preferences = add(menu, bongo_i18n_get(app->i18n,
        "composables.useAppMenu.labels.preference", "Preferences"), SDL_TRAYENTRY_BUTTON,
        on_preferences, tray);
    tray->exit = add(menu, bongo_i18n_get(app->i18n,
        "composables.useAppMenu.labels.quitApp", "Exit"), SDL_TRAYENTRY_BUTTON, on_exit, tray);
    bongo_tray_sync(tray);
    return tray;
}

void bongo_tray_sync(BongoTray *tray) {
    if (!tray) return;
    SDL_SetTrayEntryChecked(tray->visible, tray->app->config.window.visible);
    SDL_SetTrayEntryChecked(tray->pass_through, tray->app->config.window.pass_through);
    SDL_SetTrayEntryChecked(tray->always_on_top, tray->app->config.window.always_on_top);
    SDL_SetTrayEntryLabel(tray->visible, bongo_i18n_get(tray->app->i18n,
        tray->app->config.window.visible ? "composables.useAppMenu.labels.hideCat" :
        "composables.useAppMenu.labels.showCat", tray->app->config.window.visible
        ? "Hide BongoCat" : "Show BongoCat"));
    SDL_SetTrayEntryLabel(tray->pass_through, bongo_i18n_get(tray->app->i18n,
        "composables.useAppMenu.labels.passThrough", "Mouse pass-through"));
    SDL_SetTrayEntryLabel(tray->always_on_top, bongo_i18n_get(tray->app->i18n,
        "pages.preference.cat.labels.alwaysOnTop", "Always on top"));
    SDL_SetTrayEntryLabel(tray->preferences, bongo_i18n_get(tray->app->i18n,
        "composables.useAppMenu.labels.preference", "Preferences"));
    SDL_SetTrayEntryLabel(tray->exit, bongo_i18n_get(tray->app->i18n,
        "composables.useAppMenu.labels.quitApp", "Exit"));
    SDL_UpdateTrays();
}

bool bongo_tray_self_test(BongoTray *tray) {
    if (!tray || !tray->handle || !tray->visible || !tray->pass_through ||
        !tray->always_on_top || !tray->preferences || !tray->exit) return false;
    BongoApp *app = tray->app;
    bool visible = app->config.window.visible;
    bool pass_through = app->config.window.pass_through;
    bool always_on_top = app->config.window.always_on_top;
    on_visible(tray, tray->visible); on_visible(tray, tray->visible);
    on_pass_through(tray, tray->pass_through);
    on_pass_through(tray, tray->pass_through);
    on_always_on_top(tray, tray->always_on_top);
    on_always_on_top(tray, tray->always_on_top);
    on_preferences(tray, tray->preferences);
    bool preferences = bongo_preferences_visible(app->preferences);
    bongo_preferences_close(app->preferences);
    return preferences && app->config.window.visible == visible &&
        app->config.window.pass_through == pass_through &&
        app->config.window.always_on_top == always_on_top;
}

void bongo_tray_destroy(BongoTray *tray) {
    if (!tray) return;
    if (tray->handle) SDL_DestroyTray(tray->handle);
    bongo_image_free(&tray->icon);
    free(tray);
}
