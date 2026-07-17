#include "runtime.h"

#include <SDL3/SDL.h>
#include <string.h>

static void track_hover(BongoApp *app, double x, double y) {
    int window_x, window_y, width, height;
    SDL_GetWindowPosition(app->window, &window_x, &window_y);
    SDL_GetWindowSize(app->window, &width, &height);
    bool inside = x >= window_x && x <= window_x + width &&
        y >= window_y && y <= window_y + height;
    if (inside == app->hover_inside) return;
    app->hover_inside = inside;
    app->hover_deadline_ns = inside ? SDL_GetTicksNS() +
        (uint64_t)(app->config.window.hide_delay_seconds * 1000000000.0) : 0;
    if (!inside && app->hover_hidden) {
        SDL_SetWindowOpacity(app->window, app->config.window.opacity_percent / 100.0f);
        bongo_platform_set_click_through(&app->platform,
            app->config.window.pass_through);
        app->hover_hidden = false;
    }
}

void bongo_app_update_hover(BongoApp *app, uint64_t now) {
    if (!app->config.window.hide_on_hover || !app->hover_inside || app->hover_hidden ||
        !app->hover_deadline_ns || now < app->hover_deadline_ns) return;
    SDL_SetWindowOpacity(app->window, 0.0f);
    bongo_platform_set_click_through(&app->platform, true);
    app->hover_hidden = true;
}

static void set_parameter(BongoApp *app, const char *id,
    float x_ratio, float y_ratio) {
    BongoParameterRange range;
    if (!bongo_live2d_parameter(app->live2d, id, &range)) return;
    size_t length = strlen(id);
    bool y_axis = length && id[length - 1] == 'Y';
    bool z_axis = length && id[length - 1] == 'Z';
    float value;
    if (z_axis) {
        float x = 1.0f - 2.0f * x_ratio;
        float y = 1.0f - 2.0f * y_ratio;
        value = x * y * range.minimum;
    } else {
        float ratio = y_axis ? y_ratio : x_ratio;
        value = range.maximum - ratio * (range.maximum - range.minimum);
    }
    if (!y_axis && app->config.model.mouse_mirror) value = -value;
    bongo_live2d_set_parameter(app->live2d, id, value);
}

void bongo_app_apply_mouse(BongoApp *app) {
    double x, y;
    if (!app || !bongo_input_take_mouse(&app->input, &x, &y)) return;
    track_hover(app, x, y);
    if (app->config.model.ignore_mouse) return;
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
