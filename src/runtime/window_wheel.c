#include "runtime.h"
#include "l2dcat/preferences.h"

#define WHEEL_OPACITY_STEP 5.0f
#define WHEEL_SCALE_STEP 5.0f
#define WHEEL_OPACITY_ANIMATION_NS 220000000ull
#define WHEEL_SCALE_ANIMATION_NS 420000000ull

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
    app->wheel_opacity_start = app->wheel_opacity_target;
    app->wheel_scale_start = app->wheel_scale_target;
    app->wheel_center_x = x + width * 0.5f;
    app->wheel_center_y = y + height * 0.5f;
    app->wheel_geometry_scale = app->config.window.scale_percent;
    app->wheel_base_width = width;
    app->wheel_base_height = height;
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
    app->wheel_opacity_start = app->config.window.opacity_percent;
    app->wheel_scale_start = app->config.window.scale_percent;
    app->wheel_animation_ns = SDL_GetTicksNS();
    app->wheel_animation_active = true;
    app->dirty = true;
}

static float interpolate(float start, float target, float progress) {
    return start + (target - start) * progress;
}

static void apply_scale(L2DCatApp *app, float scale) {
    float old = app->config.window.scale_percent;
    if (old <= 0.0f || SDL_fabsf(scale - old) < 0.001f) return;
    float factor = scale / app->wheel_geometry_scale;
    int next_width = SDL_max(1, (int)(app->wheel_base_width * factor + 0.5f));
    int next_height = SDL_max(1, (int)(app->wheel_base_height * factor + 0.5f));
    app->config.window.scale_percent = scale;
    app->config.window.width = next_width;
    app->config.window.height = next_height;
    l2dcat_platform_set_geometry(&app->platform,
        (int)(app->wheel_center_x - next_width * 0.5f + 0.5f),
        (int)(app->wheel_center_y - next_height * 0.5f + 0.5f),
        next_width, next_height);
    l2dcat_window_mark_hit_dirty(app);
}

void l2dcat_window_update_wheel_animation(L2DCatApp *app, uint64_t now) {
    if (!app || !app->wheel_animation_active) return;
    uint64_t duration = SDL_fabsf(app->wheel_scale_target - app->wheel_scale_start) > 0.01f
        ? WHEEL_SCALE_ANIMATION_NS : WHEEL_OPACITY_ANIMATION_NS;
    float progress = SDL_clamp((float)(now - app->wheel_animation_ns) /
        (float)duration, 0.0f, 1.0f);
    float opacity = interpolate(app->wheel_opacity_start,
        app->wheel_opacity_target, progress);
    float scale = interpolate(app->wheel_scale_start,
        app->wheel_scale_target, progress);
    app->config.window.opacity_percent = opacity;
    if (!app->hover_hidden) SDL_SetWindowOpacity(app->window, opacity / 100.0f);
    apply_scale(app, scale);
    app->wheel_animation_active = progress < 1.0f;
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
    uint64_t started = app->wheel_animation_ns;
    for (int i = 1; i <= 30; ++i)
        l2dcat_window_update_wheel_animation(app, started + i * 16666667ull);
    bool opacity = SDL_fabsf(app->config.window.opacity_percent - 75.0f) < 0.1f;
    SDL_SetModState(modifiers | SDL_KMOD_CTRL);
    wheel.y = 1.0f; l2dcat_window_wheel(app, &wheel);
    started = app->wheel_animation_ns;
    for (int i = 1; i <= 30; ++i)
        l2dcat_window_update_wheel_animation(app, started + i * 16666667ull);
    bool scale = SDL_fabsf(app->config.window.scale_percent - 105.0f) < 0.1f;
    SDL_SetModState(modifiers);
    app->config.window = backup;
    app->wheel_animation_active = false;
    SDL_SetWindowOpacity(app->window, backup.opacity_percent / 100.0f);
    l2dcat_platform_set_geometry(&app->platform, original_x, original_y,
        backup.width, backup.height);
    return opacity && scale;
}
