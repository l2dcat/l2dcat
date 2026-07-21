#include "l2dcat/platform.h"
#include "windows_borderless.h"
#include "windows_keys.h"

#ifdef _WIN32
#include <SDL3/SDL.h>
#include <SDL3/SDL_properties.h>
#include <SDL3/SDL_video.h>
#include <dwmapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

typedef struct WindowsState {
    L2DCatPlatform *platform;
    HANDLE thread;
    HANDLE ready;
    DWORD thread_id;
    HHOOK keyboard;
    HHOOK mouse;
    bool hooks_ready;
} WindowsState;

static WindowsState *global_state;
static HANDLE instance_mutex;

static HWND native_window(L2DCatPlatform *platform) {
    return (HWND)SDL_GetPointerProperty(SDL_GetWindowProperties(platform->window),
        SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
}

static void push_event(L2DCatInputKind kind, const char *name, float value) {
    if (!global_state || !global_state->platform || !name) return;
    L2DCatInputEvent event = {0};
    event.kind = kind;
    event.timestamp_ms = GetTickCount64();
    event.value = value;
    snprintf(event.name, sizeof(event.name), "%s", name);
    if (l2dcat_input_push(global_state->platform->input, &event)) {
        SDL_Event wake = {.type = SDL_EVENT_USER};
        SDL_PushEvent(&wake);
    }
}

static LRESULT CALLBACK keyboard_hook(int code, WPARAM message, LPARAM data) {
    if (code == HC_ACTION) {
        const KBDLLHOOKSTRUCT *key = (const KBDLLHOOKSTRUCT *)data;
        bool down = message == WM_KEYDOWN || message == WM_SYSKEYDOWN;
        bool up = message == WM_KEYUP || message == WM_SYSKEYUP;
        char buffer[16];
        const char *name = l2dcat_windows_key_name(key, buffer);
        if (name && (down || up)) push_event(down ? L2DCAT_INPUT_KEY_DOWN : L2DCAT_INPUT_KEY_UP,
            name, down ? 1.0f : 0.0f);
    }
    return CallNextHookEx(NULL, code, message, data);
}

static const char *mouse_button(WPARAM message, DWORD mouse_data) {
    switch (message) {
    case WM_LBUTTONDOWN: case WM_LBUTTONUP: return "Left";
    case WM_RBUTTONDOWN: case WM_RBUTTONUP: return "Right";
    case WM_MBUTTONDOWN: case WM_MBUTTONUP: return "Middle";
    case WM_XBUTTONDOWN: case WM_XBUTTONUP:
        return HIWORD(mouse_data) == XBUTTON1 ? "Back" : "Forward";
    default: return NULL;
    }
}

static LRESULT CALLBACK mouse_hook(int code, WPARAM message, LPARAM data) {
    if (code == HC_ACTION && global_state) {
        const MSLLHOOKSTRUCT *mouse = (const MSLLHOOKSTRUCT *)data;
        if (message == WM_MOUSEMOVE) {
            l2dcat_input_mouse(global_state->platform->input, mouse->pt.x, mouse->pt.y);
        } else {
            const char *name = mouse_button(message, mouse->mouseData);
            bool down = message == WM_LBUTTONDOWN || message == WM_RBUTTONDOWN ||
                message == WM_MBUTTONDOWN || message == WM_XBUTTONDOWN;
            if (name) push_event(down ? L2DCAT_INPUT_MOUSE_DOWN : L2DCAT_INPUT_MOUSE_UP,
                name, down ? 1.0f : 0.0f);
        }
    }
    return CallNextHookEx(NULL, code, message, data);
}

static DWORD WINAPI hook_thread(void *context) {
    WindowsState *state = context;
    global_state = state;
    MSG message;
    PeekMessageW(&message, NULL, WM_USER, WM_USER, PM_NOREMOVE);
    state->keyboard = SetWindowsHookExW(WH_KEYBOARD_LL, keyboard_hook, NULL, 0);
    state->mouse = SetWindowsHookExW(WH_MOUSE_LL, mouse_hook, NULL, 0);
    state->hooks_ready = state->keyboard && state->mouse;
    SetEvent(state->ready);
    while (GetMessageW(&message, NULL, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    if (state->keyboard) UnhookWindowsHookEx(state->keyboard);
    if (state->mouse) UnhookWindowsHookEx(state->mouse);
    global_state = NULL;
    return 0;
}

L2DCatResult l2dcat_platform_init(L2DCatPlatform *platform, SDL_Window *window,
    L2DCatInputState *input, L2DCatError *error) {
    if (!platform || !window || !input) return L2DCAT_ERROR_ARGUMENT;
    memset(platform, 0, sizeof(*platform));
    platform->window = window;
    platform->input = input;
    WindowsState *state = calloc(1, sizeof(*state));
    if (!state) return L2DCAT_ERROR_MEMORY;
    state->platform = platform;
    state->ready = CreateEventW(NULL, TRUE, FALSE, NULL);
    state->thread = state->ready
        ? CreateThread(NULL, 0, hook_thread, state, 0, &state->thread_id) : NULL;
    platform->native = state;
    if (!state->ready || !state->thread || WaitForSingleObject(state->ready, 3000) != WAIT_OBJECT_0 ||
        !state->hooks_ready) {
        l2dcat_error_set(error, L2DCAT_ERROR_PLATFORM, "Global input hooks failed");
        l2dcat_platform_shutdown(platform);
        return L2DCAT_ERROR_PLATFORM;
    }
    HWND hwnd = native_window(platform);
    l2dcat_windows_borderless_install(hwnd);
    MARGINS margins = {-1, -1, -1, -1};
    DwmExtendFrameIntoClientArea(hwnd, &margins);
    SetWindowPos(hwnd, NULL, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE |
        SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    return L2DCAT_OK;
}

void l2dcat_platform_shutdown(L2DCatPlatform *platform) {
    WindowsState *state = platform ? platform->native : NULL;
    if (!state) return;
    HWND window = native_window(platform);
    if (window) l2dcat_windows_borderless_uninstall(window);
    if (state->thread_id) PostThreadMessageW(state->thread_id, WM_QUIT, 0, 0);
    if (state->thread) WaitForSingleObject(state->thread, 3000);
    if (state->thread) CloseHandle(state->thread);
    if (state->ready) CloseHandle(state->ready);
    free(state);
    platform->native = NULL;
}

void l2dcat_platform_set_always_on_top(L2DCatPlatform *platform, bool enabled) {
    SetWindowPos(native_window(platform), enabled ? HWND_TOPMOST : HWND_NOTOPMOST,
        0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

void l2dcat_platform_begin_drag(L2DCatPlatform *platform) {
    HWND hwnd = native_window(platform);
    ReleaseCapture();
    SendMessageW(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
}

bool l2dcat_platform_global_input_supported(void) { return true; }

bool l2dcat_platform_single_instance_begin(void) {
    instance_mutex = CreateMutexW(NULL, FALSE, L"Local\\l2dcat.SingleInstance");
    if (!instance_mutex) return true;
    if (GetLastError() != ERROR_ALREADY_EXISTS) return true;
    HWND existing = FindWindowW(NULL, L"l2dcat");
    if (existing) {
        ShowWindowAsync(existing, IsIconic(existing) ? SW_RESTORE : SW_SHOW);
        SetForegroundWindow(existing);
    }
    CloseHandle(instance_mutex);
    instance_mutex = NULL;
    return false;
}

void l2dcat_platform_single_instance_end(void) {
    if (instance_mutex) CloseHandle(instance_mutex);
    instance_mutex = NULL;
}

L2DCatResult l2dcat_platform_set_autostart(bool enabled, L2DCatError *error) {
    HKEY key;
    LONG result = RegCreateKeyExW(HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, NULL, 0,
        KEY_SET_VALUE, NULL, &key, NULL);
    if (result != ERROR_SUCCESS) {
        l2dcat_error_set(error, L2DCAT_ERROR_PLATFORM, "Cannot open autostart registry key");
        return L2DCAT_ERROR_PLATFORM;
    }
    if (enabled) {
        wchar_t executable[L2DCAT_PATH_CAP];
        DWORD length = GetModuleFileNameW(NULL, executable, L2DCAT_PATH_CAP);
        wchar_t command[L2DCAT_PATH_CAP + 4];
        swprintf(command, L2DCAT_PATH_CAP + 4, L"\"%ls\"", executable);
        result = RegSetValueExW(key, L"l2dcat", 0, REG_SZ,
            (const BYTE *)command, (DWORD)((wcslen(command) + 1) * sizeof(wchar_t)));
        (void)length;
    } else result = RegDeleteValueW(key, L"l2dcat");
    RegCloseKey(key);
    if (result != ERROR_SUCCESS && result != ERROR_FILE_NOT_FOUND) {
        l2dcat_error_set(error, L2DCAT_ERROR_PLATFORM, "Cannot update autostart setting");
        return L2DCAT_ERROR_PLATFORM;
    }
    return L2DCAT_OK;
}

bool l2dcat_platform_is_elevated(void) {
    BOOL elevated = FALSE;
    PSID administrators = NULL;
    SID_IDENTIFIER_AUTHORITY authority = SECURITY_NT_AUTHORITY;
    if (AllocateAndInitializeSid(&authority, 2, SECURITY_BUILTIN_DOMAIN_RID,
        DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &administrators)) {
        CheckTokenMembership(NULL, administrators, &elevated);
        FreeSid(administrators);
    }
    return elevated != FALSE;
}

static wchar_t *wide(const char *text) {
    if (!text) return NULL;
    int length = MultiByteToWideChar(CP_UTF8, 0, text, -1, NULL, 0);
    wchar_t *value = length > 0 ? calloc((size_t)length, sizeof(*value)) : NULL;
    if (value) MultiByteToWideChar(CP_UTF8, 0, text, -1, value, length);
    return value;
}

static void menu_text(HMENU menu, UINT flags, UINT_PTR id, const char *text) {
    wchar_t *label = wide(text);
    AppendMenuW(menu, flags, id, label ? label : L"");
    free(label);
}

L2DCatMenuAction l2dcat_platform_context_menu(L2DCatPlatform *platform,
    const L2DCatMenuLabels *labels) {
    if (!platform || !labels) return L2DCAT_MENU_NONE;
    HMENU menu = CreatePopupMenu(), sizes = CreatePopupMenu(), opacity = CreatePopupMenu();
    HMENU models = CreatePopupMenu();
    if (!menu || !sizes || !opacity || !models) return L2DCAT_MENU_NONE;
    menu_text(menu, MF_STRING, L2DCAT_MENU_PREFERENCES, labels->preferences);
    menu_text(menu, MF_STRING, L2DCAT_MENU_HIDE, labels->hide);
    AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
    menu_text(menu, MF_STRING | (labels->pass_through_checked ? MF_CHECKED : 0),
        L2DCAT_MENU_PASS_THROUGH, labels->pass_through);
    menu_text(menu, MF_STRING | (labels->always_on_top_checked ? MF_CHECKED : 0),
        L2DCAT_MENU_ALWAYS_ON_TOP, labels->always_on_top);
    const int scales[] = {50, 75, 100, 125, 150, 200};
    for (int i = 0; i < 6; ++i) {
        wchar_t label[16]; swprintf(label, 16, L"%d%%", scales[i]);
        UINT flags = MF_STRING | (SDL_fabsf(labels->scale_percent - scales[i]) < .5f ? MF_CHECKED : 0);
        AppendMenuW(sizes, flags, L2DCAT_MENU_SCALE_50 + i, label);
    }
    const int opacities[] = {10,20,30,40,50,60,70,80,90,100};
    for (int i = 0; i < 10; ++i) {
        wchar_t label[16]; swprintf(label, 16, L"%d%%", opacities[i]);
        UINT flags = MF_STRING | (SDL_fabsf(labels->opacity_percent - opacities[i]) < .5f ? MF_CHECKED : 0);
        AppendMenuW(opacity, flags, L2DCAT_MENU_OPACITY_10 + i, label);
    }
    for (size_t i = 0; i < labels->model_count; ++i)
        menu_text(models, MF_STRING | (i == labels->current_model ? MF_CHECKED : 0),
            L2DCAT_MENU_MODEL_FIRST + i, labels->model_names[i]);
    wchar_t *size_label = wide(labels->window_size), *opacity_label = wide(labels->opacity);
    wchar_t *model_label = wide(labels->model);
    AppendMenuW(menu, MF_POPUP, (UINT_PTR)sizes, size_label ? size_label : L"");
    AppendMenuW(menu, MF_POPUP, (UINT_PTR)opacity, opacity_label ? opacity_label : L"");
    AppendMenuW(menu, MF_POPUP | (labels->model_count ? 0 : MF_GRAYED),
        (UINT_PTR)models, model_label ? model_label : L"");
    free(size_label); free(opacity_label); free(model_label);
    AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
    menu_text(menu, MF_STRING, L2DCAT_MENU_EXIT, labels->exit);
    POINT point; GetCursorPos(&point);
    HWND window = native_window(platform);
    SetForegroundWindow(window);
    l2dcat_windows_menu_preview(labels->preview, labels->preview_userdata);
    UINT command = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON,
        point.x, point.y, 0, window, NULL);
    l2dcat_windows_menu_preview(NULL, NULL);
    if (labels->restore) labels->restore(labels->preview_userdata, (L2DCatMenuAction)command);
    DestroyMenu(menu);
    return (L2DCatMenuAction)command;
}

bool l2dcat_platform_restart(void) {
    wchar_t executable[L2DCAT_PATH_CAP], command[L2DCAT_PATH_CAP + 4];
    if (!GetModuleFileNameW(NULL, executable, L2DCAT_PATH_CAP)) return false;
    swprintf(command, L2DCAT_PATH_CAP + 4, L"\"%ls\"", executable);
    STARTUPINFOW startup = {.cb = sizeof(startup)};
    PROCESS_INFORMATION process = {0};
    bool ok = CreateProcessW(executable, command, NULL, NULL, FALSE, 0,
        NULL, NULL, &startup, &process) != FALSE;
    if (ok) { CloseHandle(process.hThread); CloseHandle(process.hProcess); }
    return ok;
}
#endif
