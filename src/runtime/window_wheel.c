#include "runtime.h"
#include "l2dcat/preferences.h"

#define WHEEL_OPACITY_STEP 5.0f
#define WHEEL_SCALE_STEP 5.0f
#define WHEEL_SCALE_TARGET_LEAD 15.0f
#define WHEEL_OPACITY_ANIMATION_NS 220000000ull
#define WHEEL_SCALE_ANIMATION_NS 420000000ull

static float wheel_delta(const SDL_MouseWheelEvent *event) {
    float value = event ? event->y : 0.0f;
    return event && event->direction == SDL_MOUSEWHEEL_FLIPPED ? -value : value;
}

static bool initialize_targets(L2DCatApp *app) {
    if (app->wheel_animation_active) return true;
    int x = 0, y = 0, width = 0, height = 0;
    if (!SDL_GetWindowPosition(app->window, &x, &y) ||
        !SDL_GetWindowSize(app->window, &width, &height) ||
        width <= 0 || height <= 0 || app->config.window.scale_percent <= 0.0f)
        return false;
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
    return true;
}

static int round_position(float value) {
    return (int)(value + (value < 0.0f ? -0.5f : 0.5f));
}

static void clamp_position(L2DCatApp *app, int width, int height, int *x, int *y) {
    if (!app->config.window.keep_in_screen) return;
    SDL_DisplayID display = SDL_GetDisplayForWindow(app->window);
    SDL_Rect bounds;
    if (!display || !SDL_GetDisplayUsableBounds(display, &bounds)) return;
    int max_x = SDL_max(bounds.x, bounds.x + bounds.w - width);
    int max_y = SDL_max(bounds.y, bounds.y + bounds.h - height);
    *x = SDL_clamp(*x, bounds.x, max_x);
    *y = SDL_clamp(*y, bounds.y, max_y);
}

void l2dcat_window_wheel(L2DCatApp *app, const SDL_MouseWheelEvent *event) {
    if (!app || !app->window || !event) return;
    float delta = wheel_delta(event);
    if (SDL_fabsf(delta) < 0.001f) return;
    delta = SDL_clamp(delta, -1.0f, 1.0f);
    if (!initialize_targets(app)) return;
    float old_opacity_target = app->wheel_opacity_target;
    float old_scale_target = app->wheel_scale_target;
    if (SDL_GetModState() & SDL_KMOD_CTRL) {
        float current = app->config.window.scale_percent;
        float minimum = SDL_max(10.0f, current - WHEEL_SCALE_TARGET_LEAD);
        float maximum = SDL_min(500.0f, current + WHEEL_SCALE_TARGET_LEAD);
        app->wheel_scale_target = SDL_clamp(app->wheel_scale_target +
            delta * WHEEL_SCALE_STEP, minimum, maximum);
    } else {
        app->wheel_opacity_target = SDL_clamp(app->wheel_opacity_target +
            delta * WHEEL_OPACITY_STEP, 10.0f, 100.0f);
    }
    if (old_opacity_target == app->wheel_opacity_target &&
        old_scale_target == app->wheel_scale_target) return;
    if (SDL_fabsf(app->wheel_opacity_target - app->config.window.opacity_percent) < 0.001f &&
        SDL_fabsf(app->wheel_scale_target - app->config.window.scale_percent) < 0.001f) {
        app->wheel_animation_active = false;
        return;
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
    int next_x = round_position(app->wheel_center_x - next_width * 0.5f);
    int next_y = round_position(app->wheel_center_y - next_height * 0.5f);
    clamp_position(app, next_width, next_height, &next_x, &next_y);
    l2dcat_platform_set_geometry(&app->platform,
        next_x, next_y, next_width, next_height);
    if (SDL_GetWindowSizeInPixels(app->window,
        &app->resize_pixel_width, &app->resize_pixel_height))
        app->resize_pending = true;
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
    int original_x, original_y, original_width, original_height;
    SDL_GetWindowPosition(app->window, &original_x, &original_y);
    SDL_GetWindowSize(app->window, &original_width, &original_height);
    SDL_Keymod modifiers = SDL_GetModState();
    SDL_SetModState(modifiers & ~SDL_KMOD_CTRL);
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
    app->wheel_animation_active = false;
    app->config.window.scale_percent = 100.0f;
    wheel.y = 1000.0f;
    for (int i = 0; i < 32; ++i) l2dcat_window_wheel(app, &wheel);
    started = app->wheel_animation_ns;
    for (int i = 1; i <= 30; ++i)
        l2dcat_window_update_wheel_animation(app, started + i * 16666667ull);
    bool burst = app->config.window.scale_percent <= 115.1f;
    app->wheel_animation_active = false;
    app->config.window.scale_percent = 100.0f;
    wheel.y = 1.0f;
    l2dcat_window_wheel(app, &wheel);
    wheel.y = -1.0f;
    l2dcat_window_wheel(app, &wheel);
    bool reversal = !app->wheel_animation_active;
    wheel.y = 1.0f;
    wheel.direction = SDL_MOUSEWHEEL_FLIPPED;
    l2dcat_window_wheel(app, &wheel);
    started = app->wheel_animation_ns;
    for (int i = 1; i <= 30; ++i)
        l2dcat_window_update_wheel_animation(app, started + i * 16666667ull);
    bool flipped = SDL_fabsf(app->config.window.scale_percent - 95.0f) < 0.1f;
    wheel.direction = SDL_MOUSEWHEEL_NORMAL;
    app->wheel_animation_active = false;
    app->config.window.scale_percent = 500.0f;
    l2dcat_window_wheel(app, &wheel);
    bool maximum = !app->wheel_animation_active;
    app->config.window.scale_percent = 10.0f;
    wheel.y = -1.0f;
    l2dcat_window_wheel(app, &wheel);
    bool minimum = !app->wheel_animation_active;
    SDL_SetModState(modifiers & ~SDL_KMOD_CTRL);
    app->config.window.opacity_percent = 100.0f;
    wheel.y = 1.0f;
    l2dcat_window_wheel(app, &wheel);
    bool opacity_maximum = !app->wheel_animation_active;
    app->config.window.opacity_percent = 10.0f;
    wheel.y = -1.0f;
    l2dcat_window_wheel(app, &wheel);
    bool opacity_minimum = !app->wheel_animation_active;
    bool rounding = round_position(-10.6f) == -11 && round_position(10.6f) == 11;
    SDL_SetModState(modifiers);
    app->config.window = backup;
    app->wheel_animation_active = false;
    SDL_SetWindowOpacity(app->window, backup.opacity_percent / 100.0f);
    l2dcat_platform_set_geometry(&app->platform, original_x, original_y,
        original_width, original_height);
    return opacity && scale && burst && reversal && flipped && maximum && minimum &&
        opacity_maximum && opacity_minimum && rounding;
}
