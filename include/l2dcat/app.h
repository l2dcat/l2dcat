#ifndef L2DCAT_APP_H
#define L2DCAT_APP_H

#include "l2dcat/config.h"
#include "l2dcat/input.h"
#include "l2dcat/model.h"
#include "l2dcat/mouse.h"
#include "l2dcat/platform.h"
#include "l2dcat/shortcut.h"

typedef struct L2DCatAudio L2DCatAudio;
typedef struct L2DCatTray L2DCatTray;
typedef struct L2DCatOverlay L2DCatOverlay;
typedef struct L2DCatPreferences L2DCatPreferences;
typedef struct L2DCatI18n L2DCatI18n;

typedef struct L2DCatApp {
    L2DCatConfig config;
    L2DCatInputState input;
    L2DCatShortcutState shortcut_state;
    L2DCatModelCatalog models;
    L2DCatBehaviorCatalog behaviors;
    L2DCatI18n *i18n;
    L2DCatPlatform platform;
    L2DCatLive2D *live2d;
    L2DCatAudio *audio;
    L2DCatTray *tray;
    L2DCatOverlay *overlay;
    L2DCatPreferences *preferences;
    SDL_Window *window;
    void *gl_context;
    char config_path[L2DCAT_PATH_CAP];
    char data_root[L2DCAT_PATH_CAP];
    char asset_root[L2DCAT_PATH_CAP];
    char locale_root[L2DCAT_PATH_CAP];
    char smoke_import_path[L2DCAT_PATH_CAP];
    char smoke_model[L2DCAT_ID_CAP];
    char smoke_live2d_scenario[L2DCAT_ID_CAP];
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
    bool smoke_frame_series;
    bool restart_requested;
    int smoke_language;
    int smoke_theme;
    int smoke_preference_page;
    int exit_code;
    bool dirty;
    uint64_t last_frame_ns;
    uint64_t smoke_deadline_ns;
    uint64_t hover_deadline_ns;
    uint64_t pointer_hit_deadline_ns;
    uint64_t mouse_last_ns;
    uint64_t frame_audit_bmp_ns;
    L2DCatMouseTracking mouse_tracking;
    bool hover_inside;
    bool hover_hidden;
    bool pointer_known;
    bool pointer_hit_dirty;
    bool pointer_transparent;
    bool click_through_valid;
    bool click_through_applied;
    bool left_mouse_down;
    bool right_mouse_down;
    double pointer_x, pointer_y;
    bool resize_gesture;
    float resize_scale_start, resize_scale_target;
    int resize_base_width, resize_base_height;
    bool resize_pending;
    bool wheel_animation_active;
    bool wheel_gesture_active;
    int resize_pixel_width, resize_pixel_height;
    uint64_t wheel_animation_ns, wheel_input_ns, wheel_event_ns;
    float wheel_opacity_target, wheel_scale_target;
    float wheel_opacity_start, wheel_scale_start;
    float wheel_center_x, wheel_center_y;
    float wheel_geometry_scale;
    int wheel_base_width, wheel_base_height;
    bool drag_candidate;
    float drag_start_x, drag_start_y;
    float left_stick_x, left_stick_y;
    float right_stick_x, right_stick_y;
    bool left_stick_pressed, right_stick_pressed;
} L2DCatApp;

int l2dcat_app_run(int argc, char **argv);
void l2dcat_app_apply_input(L2DCatApp *app, const L2DCatInputEvent *event);
void l2dcat_app_reset_gamepad(L2DCatApp *app);
void l2dcat_gamepad_event(L2DCatApp *app, const void *sdl_event);
void l2dcat_app_shortcuts(L2DCatApp *app, const L2DCatInputEvent *event);
bool l2dcat_app_select_model(L2DCatApp *app, const char *id);
L2DCatResult l2dcat_app_import_model(L2DCatApp *app, const char *source, L2DCatError *error);
L2DCatResult l2dcat_app_remove_model(L2DCatApp *app, const char *id, L2DCatError *error);
void l2dcat_app_rescan_models(L2DCatApp *app);

#endif
