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
bool l2dcat_window_visible_at_pointer(L2DCatApp *app, float x, float y);
void l2dcat_window_mark_hit_dirty(L2DCatApp *app);
void l2dcat_window_schedule_pointer_hit(L2DCatApp *app);
void l2dcat_window_schedule_hit_check(L2DCatApp *app);
int l2dcat_window_wait_timeout(const L2DCatApp *app, uint64_t now);
bool l2dcat_window_wait_timeout_self_test(void);
void l2dcat_window_sync_click_through(L2DCatApp *app);
void l2dcat_window_apply_pending_resize(L2DCatApp *app);
void l2dcat_window_wheel(L2DCatApp *app, const SDL_MouseWheelEvent *event);
void l2dcat_window_update_wheel_animation(L2DCatApp *app, uint64_t now);
void l2dcat_window_cancel_wheel_animation(L2DCatApp *app);
bool l2dcat_window_wheel_self_test(L2DCatApp *app);
bool l2dcat_window_scaled_size(int base_width, int base_height, float base_scale,
    float requested_scale, float *actual_scale, int *width, int *height);
bool l2dcat_window_apply_geometry(L2DCatApp *app, int x, int y,
    float scale, int width, int height);
bool l2dcat_window_set_scale(L2DCatApp *app, float scale);
void l2dcat_window_clamp_to_display(L2DCatApp *app);
void l2dcat_window_resize_by_pointer(L2DCatApp *app, const SDL_Event *event);
const char *l2dcat_gamepad_axis_name(Uint8 axis);
const char *l2dcat_gamepad_button_name(Uint8 button);
void l2dcat_gamepads_set_enabled(L2DCatApp *app, bool enabled);
void l2dcat_app_reset_gamepad(L2DCatApp *app);
void l2dcat_app_apply_mouse(L2DCatApp *app);
void l2dcat_app_apply_mouse_position(L2DCatApp *app, double x, double y,
    float elapsed_seconds);
void l2dcat_app_track_hover(L2DCatApp *app, double x, double y);
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
void l2dcat_app_render_now(L2DCatApp *app);
void l2dcat_runtime_flow_update(L2DCatApp *app, uint64_t now);

#endif
