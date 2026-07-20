#include "runtime.h"
#include "l2dcat/preferences.h"

#define WHEEL_OPACITY_STEP 5.0f
#define WHEEL_SCALE_STEP 8.0f
#define WHEEL_EASING_SPEED 18.0f

static float wheel_delta(const SDL_MouseWheelEvent *event) {
    float value = event ? event->y : 0.0f;
    return event && event->direction == SDL_MOUSEWHEEL_FLIPPED ? -value : value;
}

static void initialize_targets(L2DCatApp *app) {
    if (app->wheel_animation_active) return;
    int x, y, width, height;
    SDL_GetWindowPosition(app->window, &x, &y);
    SDL_GetWindowSize(app->window, &width, &height);
    app->wheel_opacity_target = app->config.window.opacity_percent;
    app->wheel_scale_target = app->config.window.scale_percent;
    app->wheel_center_x = x + width * 0.5f;
    app->wheel_center_y = y + height * 0.5f;
    app->wheel_animation_ns = SDL_GetTicksNS();
}

void l2dcat_window_wheel(L2DCatApp *app, const SDL_MouseWheelEvent *event) {
    if (!app || !app->window || !event) return;
    float delta = wheel_delta(event);
    if (SDL_fabsf(delta) < 0.001f) return;
    initialize_targets(app);
    if (SDL_GetModState() & SDL_KMOD_CTRL) {
        app->wheel_scale_target = SDL_clamp(app->wheel_scale_target +
            delta * WHEEL_SCALE_STEP, 10.0f, 500.0f);
    } else {
        app->wheel_opacity_target = SDL_clamp(app->wheel_opacity_target +
            delta * WHEEL_OPACITY_STEP, 10.0f, 100.0f);
    }
    app->wheel_animation_active = true;
    app->dirty = true;
}

static bool approach(float *value, float target, float blend) {
    float difference = target - *value;
    if (SDL_fabsf(difference) < 0.02f) {
        *value = target;
        return false;
    }
    *value += difference * blend;
    return true;
}

static void apply_scale(L2DCatApp *app, float scale) {
    float old = app->config.window.scale_percent;
    if (old <= 0.0f || SDL_fabsf(scale - old) < 0.001f) return;
    int width, height;
    SDL_GetWindowSize(app->window, &width, &height);
    float factor = scale / old;
    int next_width = SDL_max(1, (int)(width * factor + 0.5f));
    int next_height = SDL_max(1, (int)(height * factor + 0.5f));
    app->config.window.scale_percent = scale;
    app->config.window.width = next_width;
    app->config.window.height = next_height;
    SDL_SetWindowSize(app->window, next_width, next_height);
    SDL_SetWindowPosition(app->window,
        (int)(app->wheel_center_x - next_width * 0.5f + 0.5f),
        (int)(app->wheel_center_y - next_height * 0.5f + 0.5f));
    l2dcat_window_mark_hit_dirty(app);
}

void l2dcat_window_update_wheel_animation(L2DCatApp *app, uint64_t now) {
    if (!app || !app->wheel_animation_active) return;
    float elapsed = (float)((now - app->wheel_animation_ns) / 1000000000.0);
    app->wheel_animation_ns = now;
    float blend = SDL_clamp(elapsed * WHEEL_EASING_SPEED, 0.0f, 1.0f);
    float opacity = app->config.window.opacity_percent;
    float scale = app->config.window.scale_percent;
    bool opacity_active = approach(&opacity, app->wheel_opacity_target, blend);
    bool scale_active = approach(&scale, app->wheel_scale_target, blend);
    app->config.window.opacity_percent = opacity;
    if (!app->hover_hidden) SDL_SetWindowOpacity(app->window, opacity / 100.0f);
    apply_scale(app, scale);
    app->wheel_animation_active = opacity_active || scale_active;
    app->dirty = true;
    l2dcat_preferences_invalidate(app->preferences);
}

void l2dcat_window_cancel_wheel_animation(L2DCatApp *app) {
    if (app) app->wheel_animation_active = false;
}

bool l2dcat_window_wheel_self_test(L2DCatApp *app) {
    if (!app || !app->window) return false;
    L2DCatWindowOptions backup = app->config.window;
    int original_x, original_y;
    SDL_GetWindowPosition(app->window, &original_x, &original_y);
    SDL_Keymod modifiers = SDL_GetModState();
    app->config.window.opacity_percent = 80.0f;
    app->config.window.scale_percent = 100.0f;
    app->wheel_animation_active = false;
    SDL_MouseWheelEvent wheel = {.y = -1.0f};
    l2dcat_window_wheel(app, &wheel);
    for (int i = 1; i <= 30; ++i)
        l2dcat_window_update_wheel_animation(app, app->wheel_animation_ns + 16666667ull);
    bool opacity = SDL_fabsf(app->config.window.opacity_percent - 75.0f) < 0.1f;
    SDL_SetModState(modifiers | SDL_KMOD_CTRL);
    wheel.y = 1.0f; l2dcat_window_wheel(app, &wheel);
    for (int i = 1; i <= 30; ++i)
        l2dcat_window_update_wheel_animation(app, app->wheel_animation_ns + 16666667ull);
    bool scale = SDL_fabsf(app->config.window.scale_percent - 108.0f) < 0.1f;
    SDL_SetModState(modifiers);
    app->config.window = backup;
    app->wheel_animation_active = false;
    SDL_SetWindowOpacity(app->window, backup.opacity_percent / 100.0f);
    SDL_SetWindowSize(app->window, backup.width, backup.height);
    SDL_SetWindowPosition(app->window, original_x, original_y);
    return opacity && scale;
}
