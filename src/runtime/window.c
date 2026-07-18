#include "runtime.h"
#include "l2dcat/i18n.h"
#include "l2dcat/preferences.h"

#include <SDL3/SDL_opengl.h>

static bool set_gl_attributes(void) {
#ifdef __APPLE__
    const int major = 4, minor = 1, profile = SDL_GL_CONTEXT_PROFILE_CORE;
#elif defined(L2DCAT_HAS_CUBISM)
    const int major = 3, minor = 3, profile = SDL_GL_CONTEXT_PROFILE_COMPATIBILITY;
#else
    const int major = 3, minor = 3, profile = SDL_GL_CONTEXT_PROFILE_CORE;
#endif
    return SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, major) &&
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, minor) &&
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, profile) &&
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1) &&
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0) &&
        SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8) &&
        SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
}

L2DCatResult l2dcat_window_create(L2DCatApp *app, L2DCatError *error) {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD | SDL_INIT_EVENTS)) {
        l2dcat_error_set(error, L2DCAT_ERROR_PLATFORM, "SDL initialization failed: %s", SDL_GetError());
        return L2DCAT_ERROR_PLATFORM;
    }
    if (!set_gl_attributes()) {
        l2dcat_error_set(error, L2DCAT_ERROR_PLATFORM, "OpenGL attributes failed: %s", SDL_GetError());
        return L2DCAT_ERROR_PLATFORM;
    }
    SDL_WindowFlags flags = SDL_WINDOW_OPENGL | SDL_WINDOW_TRANSPARENT |
        SDL_WINDOW_BORDERLESS | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    app->window = SDL_CreateWindow(L2DCAT_NAME, app->config.window.width,
        app->config.window.height, flags);
    if (!app->window) {
        l2dcat_error_set(error, L2DCAT_ERROR_PLATFORM, "Window creation failed: %s", SDL_GetError());
        return L2DCAT_ERROR_PLATFORM;
    }
    app->gl_context = SDL_GL_CreateContext(app->window);
    if (!app->gl_context || !SDL_GL_MakeCurrent(app->window, app->gl_context)) {
        l2dcat_error_set(error, L2DCAT_ERROR_PLATFORM, "OpenGL context failed: %s", SDL_GetError());
        return L2DCAT_ERROR_PLATFORM;
    }
    SDL_GL_SetSwapInterval(1);
    return L2DCAT_OK;
}

void l2dcat_window_apply(L2DCatApp *app) {
    L2DCatWindowOptions *value = &app->config.window;
    SDL_SetWindowOpacity(app->window, value->opacity_percent / 100.0f);
    SDL_SetWindowSize(app->window, value->width, value->height);
    if (value->x || value->y) SDL_SetWindowPosition(app->window, value->x, value->y);
    value->visible ? SDL_ShowWindow(app->window) : SDL_HideWindow(app->window);
    l2dcat_platform_set_click_through(&app->platform, value->pass_through);
    l2dcat_platform_set_always_on_top(&app->platform, value->always_on_top);
    l2dcat_platform_set_taskbar(&app->platform, value->taskbar_visible);
}

static void clamp_to_display(L2DCatApp *app) {
    if (!app->config.window.keep_in_screen) return;
    SDL_DisplayID display = SDL_GetDisplayForWindow(app->window);
    SDL_Rect bounds;
    if (!display || !SDL_GetDisplayUsableBounds(display, &bounds)) return;
    int x, y, width, height;
    SDL_GetWindowPosition(app->window, &x, &y);
    SDL_GetWindowSize(app->window, &width, &height);
    int max_x = bounds.x + bounds.w - width;
    int max_y = bounds.y + bounds.h - height;
    int next_x = SDL_clamp(x, bounds.x, max_x < bounds.x ? bounds.x : max_x);
    int next_y = SDL_clamp(y, bounds.y, max_y < bounds.y ? bounds.y : max_y);
    if (next_x != x || next_y != y) SDL_SetWindowPosition(app->window, next_x, next_y);
}

static void resize_by_pointer(L2DCatApp *app, const SDL_Event *event) {
    if (!(event->motion.state & SDL_BUTTON_RMASK) || !(SDL_GetModState() & SDL_KMOD_SHIFT))
        return;
    app->resize_gesture = true;
    float delta = (event->motion.xrel + event->motion.yrel) * 0.5f;
    float old_scale = app->config.window.scale_percent;
    float next_scale = SDL_clamp(old_scale + delta, 10.0f, 500.0f);
    if (next_scale == old_scale) return;
    float factor = next_scale / old_scale;
    app->config.window.scale_percent = next_scale;
    app->config.window.width = (int)(app->config.window.width * factor);
    app->config.window.height = (int)(app->config.window.height * factor);
    SDL_SetWindowSize(app->window, app->config.window.width, app->config.window.height);
}

static void begin_drag_candidate(L2DCatApp *app, const SDL_MouseButtonEvent *event) {
    app->drag_candidate = l2dcat_window_visible_at_pointer(app, event->x, event->y);
    app->drag_start_x = event->x;
    app->drag_start_y = event->y;
}

static void update_drag_candidate(L2DCatApp *app, const SDL_MouseMotionEvent *event) {
    if (!app->drag_candidate) return;
    if (!(event->state & SDL_BUTTON_LMASK)) { app->drag_candidate = false; return; }
    float x = event->x - app->drag_start_x, y = event->y - app->drag_start_y;
    if (x * x + y * y < 9.0f) return;
    app->drag_candidate = false;
    l2dcat_platform_begin_drag(&app->platform);
}

static const char *tr(L2DCatApp *app, const char *key, const char *fallback) {
    return l2dcat_i18n_get(app->i18n, key, fallback);
}

static void set_scale(L2DCatApp *app, float scale) {
    float old = app->config.window.scale_percent;
    if (old <= 0.0f || old == scale) return;
    float factor = scale / old;
    app->config.window.scale_percent = scale;
    app->config.window.width = (int)(app->config.window.width * factor);
    app->config.window.height = (int)(app->config.window.height * factor);
    SDL_SetWindowSize(app->window, app->config.window.width, app->config.window.height);
}

static void context_menu(L2DCatApp *app) {
    L2DCatMenuLabels labels = {
        tr(app, "composables.useAppMenu.labels.preference", "Preferences"),
        tr(app, "composables.useAppMenu.labels.hideCat", "Hide Cat"),
        tr(app, "composables.useAppMenu.labels.passThrough", "Pass Through"),
        tr(app, "pages.preference.cat.labels.alwaysOnTop", "Always on Top"),
        tr(app, "composables.useAppMenu.labels.windowSize", "Window Size"),
        tr(app, "composables.useAppMenu.labels.opacity", "Opacity"),
        tr(app, "composables.useAppMenu.labels.restartApp", "Restart"),
        tr(app, "composables.useAppMenu.labels.quitApp", "Exit"),
        app->config.window.pass_through, app->config.window.always_on_top};
    L2DCatMenuAction action = l2dcat_platform_context_menu(&app->platform, &labels);
    l2dcat_window_menu_action(app, action);
}

void l2dcat_window_show_context_menu(L2DCatApp *app) { context_menu(app); }

void l2dcat_window_menu_action(L2DCatApp *app, L2DCatMenuAction action) {
    if (action == L2DCAT_MENU_PREFERENCES) l2dcat_preferences_show(app->preferences);
    else if (action == L2DCAT_MENU_HIDE) {
        app->config.window.visible = false; SDL_HideWindow(app->window);
    } else if (action == L2DCAT_MENU_PASS_THROUGH) {
        app->config.window.pass_through = !app->config.window.pass_through;
        l2dcat_platform_set_click_through(&app->platform, app->config.window.pass_through);
    } else if (action == L2DCAT_MENU_ALWAYS_ON_TOP) {
        app->config.window.always_on_top = !app->config.window.always_on_top;
        l2dcat_platform_set_always_on_top(&app->platform, app->config.window.always_on_top);
    } else if (action >= L2DCAT_MENU_SCALE_50 && action <= L2DCAT_MENU_SCALE_200) {
        const float scales[] = {50, 75, 100, 125, 150, 200};
        set_scale(app, scales[action - L2DCAT_MENU_SCALE_50]);
    } else if (action >= L2DCAT_MENU_OPACITY_25 && action <= L2DCAT_MENU_OPACITY_100) {
        const float values[] = {25, 50, 75, 100};
        app->config.window.opacity_percent = values[action - L2DCAT_MENU_OPACITY_25];
        SDL_SetWindowOpacity(app->window, app->config.window.opacity_percent / 100.0f);
    } else if (action == L2DCAT_MENU_RESTART) {
        app->restart_requested = true; app->running = false;
    } else if (action == L2DCAT_MENU_EXIT) app->running = false;
    l2dcat_preferences_invalidate(app->preferences);
}

bool l2dcat_window_menu_self_test(L2DCatApp *app) {
    if (!app || !app->preferences) return false;
    app->config.window.pass_through = false;
    app->config.window.always_on_top = false;
    app->config.window.scale_percent = 100.0f;
    app->config.window.opacity_percent = 100.0f;
    l2dcat_window_menu_action(app, L2DCAT_MENU_PASS_THROUGH);
    l2dcat_window_menu_action(app, L2DCAT_MENU_ALWAYS_ON_TOP);
    l2dcat_window_menu_action(app, L2DCAT_MENU_SCALE_125);
    l2dcat_window_menu_action(app, L2DCAT_MENU_OPACITY_50);
    l2dcat_window_menu_action(app, L2DCAT_MENU_PREFERENCES);
    bool result = app->config.window.pass_through && app->config.window.always_on_top &&
        app->config.window.scale_percent == 125.0f &&
        app->config.window.opacity_percent == 50.0f &&
        l2dcat_preferences_visible(app->preferences);
    l2dcat_preferences_close(app->preferences);
    return result;
}

bool l2dcat_window_geometry_self_test(L2DCatApp *app) {
    if (!app || !app->window) return false;
    L2DCatWindowOptions backup = app->config.window;
    int original_x, original_y, original_width, original_height;
    SDL_GetWindowPosition(app->window, &original_x, &original_y);
    SDL_GetWindowSize(app->window, &original_width, &original_height);
    SDL_DisplayID display = SDL_GetDisplayForWindow(app->window);
    SDL_Rect bounds;
    if (!display || !SDL_GetDisplayUsableBounds(display, &bounds)) return false;
    app->config.window.keep_in_screen = true;
    app->config.window.width = 320; app->config.window.height = 240;
    app->config.window.scale_percent = 100.0f;
    SDL_SetWindowSize(app->window, 320, 240);
    SDL_SetWindowPosition(app->window, bounds.x - 2000, bounds.y - 2000);
    SDL_SyncWindow(app->window); clamp_to_display(app); SDL_SyncWindow(app->window);
    int x, y, width, height; SDL_GetWindowPosition(app->window, &x, &y);
    bool clamped = x >= bounds.x && y >= bounds.y;
    set_scale(app, 125.0f); SDL_SyncWindow(app->window);
    SDL_GetWindowSize(app->window, &width, &height);
    bool scaled = width == 400 && height == 300;
    l2dcat_window_menu_action(app, L2DCAT_MENU_OPACITY_50);
    bool opacity = SDL_fabsf(SDL_GetWindowOpacity(app->window) - 0.5f) < 0.02f;
    app->config.window.hide_on_hover = true;
    app->config.window.hide_delay_seconds = 0.0f;
    app->config.window.pass_through = false;
    app->config.window.opacity_percent = 100.0f;
    l2dcat_input_mouse(&app->input, x + 10, y + 10); l2dcat_app_apply_mouse(app);
    l2dcat_app_update_hover(app, SDL_GetTicksNS() + 1);
    bool hidden = app->hover_hidden && SDL_GetWindowOpacity(app->window) < 0.02f;
    l2dcat_input_mouse(&app->input, bounds.x - 10, bounds.y - 10);
    l2dcat_app_apply_mouse(app);
    bool restored = !app->hover_hidden &&
        SDL_fabsf(SDL_GetWindowOpacity(app->window) - 1.0f) < 0.02f;
    app->config.window.width = 320; app->config.window.height = 240;
    app->config.window.scale_percent = 100.0f;
    SDL_SetWindowSize(app->window, 320, 240); SDL_SyncWindow(app->window);
    SDL_Keymod old_modifiers = SDL_GetModState();
    SDL_SetModState(old_modifiers | SDL_KMOD_SHIFT);
    SDL_Event motion = {.type = SDL_EVENT_MOUSE_MOTION};
    motion.motion.state = SDL_BUTTON_RMASK;
    motion.motion.xrel = 20.0f; motion.motion.yrel = 20.0f;
    resize_by_pointer(app, &motion); SDL_SyncWindow(app->window);
    SDL_GetWindowSize(app->window, &width, &height);
    bool gesture = app->resize_gesture && app->config.window.scale_percent == 120.0f &&
        width == 384 && height == 288;
    SDL_Event released = {.type = SDL_EVENT_MOUSE_BUTTON_UP};
    released.button.button = SDL_BUTTON_RIGHT; l2dcat_window_event(app, &released);
    gesture = gesture && !app->resize_gesture;
    SDL_SetModState(old_modifiers);
    app->config.window = backup;
    l2dcat_platform_set_click_through(&app->platform, backup.pass_through);
    l2dcat_platform_set_always_on_top(&app->platform, backup.always_on_top);
    l2dcat_platform_set_taskbar(&app->platform, backup.taskbar_visible);
    SDL_SetWindowOpacity(app->window, backup.opacity_percent / 100.0f);
    SDL_SetWindowSize(app->window, original_width, original_height);
    SDL_SetWindowPosition(app->window, original_x, original_y);
    SDL_SyncWindow(app->window);
    return clamped && scaled && opacity && hidden && restored && gesture;
}

bool l2dcat_window_event(L2DCatApp *app, const SDL_Event *event) {
    if (event->type >= SDL_EVENT_WINDOW_FIRST && event->type <= SDL_EVENT_WINDOW_LAST &&
        event->window.windowID != SDL_GetWindowID(app->window)) return true;
    if (event->type == SDL_EVENT_QUIT) return false;
    if (event->type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
        SDL_HideWindow(app->window);
        app->config.window.visible = false;
        return true;
    }
    if (event->type == SDL_EVENT_WINDOW_RESIZED) {
        app->config.window.width = event->window.data1;
        app->config.window.height = event->window.data2;
        clamp_to_display(app);
        app->dirty = true;
    }
    if (event->type == SDL_EVENT_WINDOW_RESIZED ||
        event->type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
        SDL_GetWindowSizeInPixels(app->window,
            &app->resize_pixel_width, &app->resize_pixel_height);
        app->resize_pending = true;
    } else if (event->type == SDL_EVENT_WINDOW_MOVED) {
        app->config.window.x = event->window.data1;
        app->config.window.y = event->window.data2;
        clamp_to_display(app);
    } else if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
        event->button.button == SDL_BUTTON_LEFT) {
        begin_drag_candidate(app, &event->button);
    } else if (event->type == SDL_EVENT_MOUSE_MOTION) {
        resize_by_pointer(app, event);
        update_drag_candidate(app, &event->motion);
    } else if (event->type == SDL_EVENT_MOUSE_BUTTON_UP &&
        event->button.button == SDL_BUTTON_LEFT) {
        app->drag_candidate = false;
    } else if (event->type == SDL_EVENT_MOUSE_BUTTON_UP &&
        event->button.button == SDL_BUTTON_RIGHT) {
        if (app->resize_gesture) app->resize_gesture = false;
        else context_menu(app);
    }
    return true;
}

void l2dcat_window_destroy(L2DCatApp *app) {
    if (app->gl_context) SDL_GL_DestroyContext(app->gl_context);
    if (app->window) SDL_DestroyWindow(app->window);
    app->gl_context = NULL;
    app->window = NULL;
    SDL_Quit();
}
