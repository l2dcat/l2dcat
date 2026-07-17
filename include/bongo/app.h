#ifndef BONGO_APP_H
#define BONGO_APP_H

#include "bongo/config.h"
#include "bongo/input.h"
#include "bongo/model.h"
#include "bongo/platform.h"
#include "bongo/shortcut.h"

typedef struct BongoAudio BongoAudio;
typedef struct BongoTray BongoTray;
typedef struct BongoOverlay BongoOverlay;
typedef struct BongoPreferences BongoPreferences;
typedef struct BongoI18n BongoI18n;

typedef struct BongoApp {
    BongoConfig config;
    BongoInputState input;
    BongoShortcutState shortcut_state;
    BongoModelCatalog models;
    BongoBehaviorCatalog behaviors;
    BongoI18n *i18n;
    BongoPlatform platform;
    BongoLive2D *live2d;
    BongoAudio *audio;
    BongoTray *tray;
    BongoOverlay *overlay;
    BongoPreferences *preferences;
    SDL_Window *window;
    void *gl_context;
    char config_path[BONGO_PATH_CAP];
    char data_root[BONGO_PATH_CAP];
    char asset_root[BONGO_PATH_CAP];
    char locale_root[BONGO_PATH_CAP];
    char smoke_import_path[BONGO_PATH_CAP];
    char smoke_model[BONGO_ID_CAP];
    char smoke_live2d_scenario[BONGO_ID_CAP];
    bool running;
    bool smoke;
    bool smoke_preferences;
    bool smoke_remove_imported;
    bool smoke_shortcuts;
    bool smoke_menu;
    bool smoke_input_audit;
    bool smoke_ignore_global_input;
    bool smoke_taskbar_visible;
    bool smoke_context_menu;
    bool smoke_frame_audited;
    bool restart_requested;
    int smoke_language;
    int smoke_theme;
    int smoke_preference_page;
    int exit_code;
    bool dirty;
    uint64_t last_frame_ns;
    uint64_t smoke_deadline_ns;
    uint64_t hover_deadline_ns;
    bool hover_inside;
    bool hover_hidden;
    bool resize_gesture;
    float left_stick_x, left_stick_y;
    float right_stick_x, right_stick_y;
    bool left_stick_pressed, right_stick_pressed;
} BongoApp;

int bongo_app_run(int argc, char **argv);
void bongo_app_apply_input(BongoApp *app, const BongoInputEvent *event);
void bongo_app_reset_gamepad(BongoApp *app);
void bongo_gamepad_event(BongoApp *app, const void *sdl_event);
void bongo_app_shortcuts(BongoApp *app, const BongoInputEvent *event);
bool bongo_app_select_model(BongoApp *app, const char *id);
BongoResult bongo_app_import_model(BongoApp *app, const char *source, BongoError *error);
BongoResult bongo_app_remove_model(BongoApp *app, const char *id, BongoError *error);
void bongo_app_rescan_models(BongoApp *app);

#endif
