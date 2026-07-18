#ifndef L2DCAT_RUNTIME_INTERNAL_H
#define L2DCAT_RUNTIME_INTERNAL_H

#include "l2dcat/app.h"
#include <SDL3/SDL.h>

#ifdef L2DCAT_HAS_CUBISM
#define L2DCAT_FRAME_WAIT(app) (1000 / (app)->config.model.max_fps)
#else
#define L2DCAT_FRAME_WAIT(app) 100
#endif

L2DCatResult l2dcat_window_create(L2DCatApp *app, L2DCatError *error);
void l2dcat_app_locate_assets(L2DCatApp *app);
void l2dcat_window_destroy(L2DCatApp *app);
void l2dcat_window_apply(L2DCatApp *app);
bool l2dcat_window_event(L2DCatApp *app, const SDL_Event *event);
const char *l2dcat_gamepad_axis_name(Uint8 axis);
const char *l2dcat_gamepad_button_name(Uint8 button);
void l2dcat_gamepads_set_enabled(L2DCatApp *app, bool enabled);
void l2dcat_app_reset_gamepad(L2DCatApp *app);
void l2dcat_app_apply_mouse(L2DCatApp *app);
void l2dcat_app_update_hover(L2DCatApp *app, uint64_t now);
L2DCatResult l2dcat_copy_directory(const char *source, const char *target,
    L2DCatError *error);
bool l2dcat_app_shortcuts_self_test(L2DCatApp *app);
void l2dcat_window_menu_action(L2DCatApp *app, L2DCatMenuAction action);
bool l2dcat_window_menu_self_test(L2DCatApp *app);
bool l2dcat_window_geometry_self_test(L2DCatApp *app);
void l2dcat_window_show_context_menu(L2DCatApp *app);
void l2dcat_live2d_audit_run(L2DCatApp *app);
void l2dcat_frame_audit(L2DCatApp *app, int width, int height);

#endif
