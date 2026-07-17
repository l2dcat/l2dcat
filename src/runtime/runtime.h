#ifndef BONGO_RUNTIME_INTERNAL_H
#define BONGO_RUNTIME_INTERNAL_H

#include "bongo/app.h"
#include <SDL3/SDL.h>

#ifdef BONGO_HAS_CUBISM
#define BONGO_FRAME_WAIT(app) (1000 / (app)->config.model.max_fps)
#else
#define BONGO_FRAME_WAIT(app) 100
#endif

BongoResult bongo_window_create(BongoApp *app, BongoError *error);
void bongo_app_locate_assets(BongoApp *app);
void bongo_window_destroy(BongoApp *app);
void bongo_window_apply(BongoApp *app);
bool bongo_window_event(BongoApp *app, const SDL_Event *event);
const char *bongo_gamepad_axis_name(Uint8 axis);
const char *bongo_gamepad_button_name(Uint8 button);
void bongo_gamepads_set_enabled(BongoApp *app, bool enabled);
void bongo_app_reset_gamepad(BongoApp *app);
void bongo_app_apply_mouse(BongoApp *app);
void bongo_app_update_hover(BongoApp *app, uint64_t now);
BongoResult bongo_copy_directory(const char *source, const char *target,
    BongoError *error);
bool bongo_app_shortcuts_self_test(BongoApp *app);
void bongo_window_menu_action(BongoApp *app, BongoMenuAction action);
bool bongo_window_menu_self_test(BongoApp *app);
bool bongo_window_geometry_self_test(BongoApp *app);
void bongo_window_show_context_menu(BongoApp *app);
void bongo_live2d_audit_run(BongoApp *app);
void bongo_frame_audit(BongoApp *app, int width, int height);

#endif
