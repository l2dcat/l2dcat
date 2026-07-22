#include "runtime.h"
#include "l2dcat/file.h"
#include "l2dcat/path.h"

#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

static void audit_mouse(L2DCatApp *app, double x, double y) {
    if (!app->smoke_input_audit) return;
    char path[L2DCAT_PATH_CAP];
    if (!l2dcat_path_join(path, sizeof(path), app->data_root, "input-audit.txt")) return;
    FILE *file = l2dcat_file_open(path, "ab");
    if (!file) return;
    fprintf(file, "mouse x=%.2f y=%.2f\n", x, y);
    fclose(file);
}

void l2dcat_app_track_hover(L2DCatApp *app, double x, double y) {
    int window_x, window_y, width, height;
    SDL_GetWindowPosition(app->window, &window_x, &window_y);
    SDL_GetWindowSize(app->window, &width, &height);
    bool inside = x >= window_x && x <= window_x + width &&
        y >= window_y && y <= window_y + height;
    app->pointer_known = true;
    app->pointer_x = x;
    app->pointer_y = y;
    l2dcat_window_schedule_pointer_hit(app);
    if (inside == app->hover_inside) return;
    app->hover_inside = inside;
    app->hover_deadline_ns = inside ? SDL_GetTicksNS() +
        (uint64_t)(app->config.window.hide_delay_seconds * 1000000000.0) : 0;
    if (!inside && app->hover_hidden) {
        SDL_SetWindowOpacity(app->window, app->config.window.opacity_percent / 100.0f);
        app->hover_hidden = false;
        l2dcat_window_sync_click_through(app);
    }
}

void l2dcat_app_update_hover(L2DCatApp *app, uint64_t now) {
    if (!app->config.window.hide_on_hover || !app->hover_inside || app->hover_hidden ||
        !app->hover_deadline_ns || now < app->hover_deadline_ns) return;
    SDL_SetWindowOpacity(app->window, 0.0f);
    app->hover_hidden = true;
    l2dcat_window_sync_click_through(app);
}

static void set_parameter(L2DCatApp *app, const char *id,
    float x_ratio, float y_ratio) {
    L2DCatParameterRange range;
    if (!l2dcat_live2d_parameter(app->live2d, id, &range)) return;
    size_t length = strlen(id);
    char axis = length ? id[length - 1] : 'X';
    float value = l2dcat_mouse_parameter_value(range.minimum, range.maximum,
        x_ratio, y_ratio, axis, app->config.model.mouse_mirror);
    l2dcat_live2d_set_parameter(app->live2d, id, value);
}

static void reconcile_button(L2DCatApp *app, bool *current, bool pressed,
    const char *parameter) {
    if (*current == pressed) return;
    *current = pressed;
    l2dcat_live2d_set_parameter(app->live2d, parameter, pressed ? 1.0f : 0.0f);
    if (!pressed) l2dcat_window_mark_hit_dirty(app);
    app->dirty = true;
}

static void apply_mouse_tracking(L2DCatApp *app, float elapsed) {
    double x, y;
    if (!l2dcat_mouse_step(&app->mouse_tracking, elapsed, &x, &y)) return;
    SDL_Point point = {(int)x, (int)y}; SDL_Rect bounds;
    SDL_DisplayID display = SDL_GetDisplayForPoint(&point);
    if (!display || !SDL_GetDisplayBounds(display, &bounds) ||
        bounds.w <= 0 || bounds.h <= 0) return;
    float x_ratio = (float)((x - bounds.x) / bounds.w);
    float y_ratio = (float)((y - bounds.y) / bounds.h);
    const char *parameters[] = {"ParamMouseX", "ParamMouseY", "ParamAngleX",
        "ParamAngleY", "ParamAngleZ", "ParamEyeBallX", "ParamEyeBallY"};
    for (size_t i = 0; i < sizeof(parameters) / sizeof(parameters[0]); ++i)
        set_parameter(app, parameters[i], x_ratio, y_ratio);
    app->dirty = true;
}

void l2dcat_app_apply_mouse_position(L2DCatApp *app, double x, double y,
    float elapsed_seconds) {
    if (!app || app->config.model.ignore_mouse) return;
    l2dcat_mouse_target(&app->mouse_tracking, x, y);
    apply_mouse_tracking(app, elapsed_seconds);
}

void l2dcat_app_apply_mouse(L2DCatApp *app) {
    if (!app) return;
    double target_x, target_y;
    bool received = l2dcat_input_take_mouse(&app->input, &target_x, &target_y);
    float global_x = 0.0f, global_y = 0.0f;
    SDL_MouseButtonFlags buttons = SDL_GetGlobalMouseState(&global_x, &global_y);
    reconcile_button(app, &app->left_mouse_down,
        (buttons & SDL_BUTTON_LMASK) != 0, "ParamMouseLeftDown");
    reconcile_button(app, &app->right_mouse_down,
        (buttons & SDL_BUTTON_RMASK) != 0, "ParamMouseRightDown");
    if (!received || target_x != global_x || target_y != global_y) {
        target_x = global_x;
        target_y = global_y;
    }
    bool moved = !app->pointer_known || app->pointer_x != target_x ||
        app->pointer_y != target_y;
    if (moved) {
        audit_mouse(app, target_x, target_y);
        l2dcat_app_track_hover(app, target_x, target_y);
        l2dcat_mouse_target(&app->mouse_tracking, target_x, target_y);
    }
    l2dcat_window_sync_click_through(app);
    uint64_t now = SDL_GetTicksNS();
    float elapsed = app->mouse_last_ns
        ? (float)((now - app->mouse_last_ns) / 1000000000.0) : 1.0f / 60.0f;
    app->mouse_last_ns = now;
    if (app->config.model.ignore_mouse) return;
    apply_mouse_tracking(app, elapsed);
}
