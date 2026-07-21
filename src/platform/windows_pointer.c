#include "l2dcat/platform.h"

#ifdef _WIN32
#include <SDL3/SDL.h>
#include <SDL3/SDL_properties.h>
#include <windows.h>

static HWND native_window(L2DCatPlatform *platform) {
    return (HWND)SDL_GetPointerProperty(SDL_GetWindowProperties(platform->window),
        SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
}

bool l2dcat_platform_pointer_local(L2DCatPlatform *platform, double screen_x,
    double screen_y, float *local_x, float *local_y) {
    if (!platform || !local_x || !local_y) return false;
    HWND window = native_window(platform);
    POINT point = {(LONG)screen_x, (LONG)screen_y};
    RECT client;
    if (!window || !ScreenToClient(window, &point) || !GetClientRect(window, &client))
        return false;
    *local_x = (float)point.x; *local_y = (float)point.y;
    return point.x >= client.left && point.x < client.right &&
        point.y >= client.top && point.y < client.bottom;
}

static void update_style(L2DCatPlatform *platform, LONG_PTR add,
    LONG_PTR remove, bool refresh_frame) {
    HWND window = native_window(platform);
    if (!window) return;
    LONG_PTR style = GetWindowLongPtrW(window, GWL_EXSTYLE);
    LONG_PTR next = (style | add) & ~remove;
    if (next == style) return;
    SetWindowLongPtrW(window, GWL_EXSTYLE, next);
    UINT flags = SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE;
    if (refresh_frame) flags |= SWP_FRAMECHANGED;
    SetWindowPos(window, NULL, 0, 0, 0, 0, flags);
}

void l2dcat_platform_set_click_through(L2DCatPlatform *platform, bool enabled) {
    update_style(platform, enabled ? WS_EX_TRANSPARENT : 0,
        enabled ? 0 : WS_EX_TRANSPARENT, false);
}

void l2dcat_platform_set_taskbar(L2DCatPlatform *platform, bool visible) {
    update_style(platform, visible ? WS_EX_APPWINDOW : WS_EX_TOOLWINDOW,
        visible ? WS_EX_TOOLWINDOW : WS_EX_APPWINDOW, true);
}

void l2dcat_platform_raise_window(SDL_Window *window) {
    if (!window) return;
    SDL_ShowWindow(window);
    HWND handle = (HWND)SDL_GetPointerProperty(SDL_GetWindowProperties(window),
        SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
    if (!handle) { SDL_RaiseWindow(window); return; }
    if (IsIconic(handle)) ShowWindow(handle, SW_RESTORE);
    HWND foreground = GetForegroundWindow();
    DWORD foreground_thread = foreground ? GetWindowThreadProcessId(foreground, NULL) : 0;
    DWORD current_thread = GetCurrentThreadId();
    bool attached = foreground_thread && foreground_thread != current_thread &&
        AttachThreadInput(current_thread, foreground_thread, TRUE);
    BringWindowToTop(handle); SetForegroundWindow(handle); SetActiveWindow(handle);
    if (attached) AttachThreadInput(current_thread, foreground_thread, FALSE);
}

bool l2dcat_platform_set_geometry(L2DCatPlatform *platform,
    int x, int y, int width, int height) {
    if (!platform || !platform->window) return false;
    int current_width, current_height;
    bool size_known = SDL_GetWindowSize(platform->window,
        &current_width, &current_height);
    int current_x, current_y;
    bool position_known = SDL_GetWindowPosition(platform->window,
        &current_x, &current_y);
    bool size_changed = !size_known || current_width != width || current_height != height;
    bool position_changed = !position_known || current_x != x || current_y != y;
    if (!size_changed && !position_changed) return true;
    HWND window = native_window(platform);
    if (!window) return false;
    if (size_changed && position_changed) {
        int midpoint_x = current_x + (x - current_x) / 2;
        int midpoint_y = current_y + (y - current_y) / 2;
        if (!SetWindowPos(window, NULL, midpoint_x, midpoint_y, 0, 0,
            SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE)) return false;
    }
    if (size_changed && !SDL_SetWindowSize(platform->window, width, height)) return false;
    RECT bounds;
    if (!GetWindowRect(window, &bounds) || !SetWindowPos(window, NULL, x, y,
        bounds.right - bounds.left, bounds.bottom - bounds.top,
        SWP_NOZORDER | SWP_NOACTIVATE)) return false;
    return true;
}
#endif
