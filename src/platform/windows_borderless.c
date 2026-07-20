#include "windows_borderless.h"

#ifdef _WIN32
static const wchar_t original_proc_property[] = L"l2dcat.BorderlessWindowProc";

static LONG_PTR borderless_style(LONG_PTR style) {
    return (style & ~(WS_CAPTION | WS_THICKFRAME | WS_SYSMENU |
        WS_MINIMIZEBOX | WS_MAXIMIZEBOX)) | WS_POPUP;
}

static LRESULT CALLBACK borderless_window_proc(HWND window, UINT message,
    WPARAM wparam, LPARAM lparam) {
    WNDPROC original = (WNDPROC)GetPropW(window, original_proc_property);
    if (message == WM_STYLECHANGING && wparam == (WPARAM)GWL_STYLE && lparam) {
        STYLESTRUCT *styles = (STYLESTRUCT *)lparam;
        styles->styleNew = (DWORD)borderless_style(styles->styleNew);
    } else if (message == WM_STYLECHANGED && wparam == (WPARAM)GWL_STYLE) {
        LONG_PTR style = GetWindowLongPtrW(window, GWL_STYLE);
        LONG_PTR fixed = borderless_style(style);
        if (fixed != style) SetWindowLongPtrW(window, GWL_STYLE, fixed);
    } else if (message == WM_NCPAINT) {
        return 0;
    } else if (message == WM_NCACTIVATE) {
        return TRUE;
    }
    return CallWindowProcW(original ? original : DefWindowProcW,
        window, message, wparam, lparam);
}

void l2dcat_windows_borderless_install(HWND window) {
    if (!window || GetPropW(window, original_proc_property)) return;
    LONG_PTR style = borderless_style(GetWindowLongPtrW(window, GWL_STYLE));
    SetWindowLongPtrW(window, GWL_STYLE, style);
    WNDPROC original = (WNDPROC)GetWindowLongPtrW(window, GWLP_WNDPROC);
    if (!original || !SetPropW(window, original_proc_property, (HANDLE)original)) return;
    SetWindowLongPtrW(window, GWLP_WNDPROC, (LONG_PTR)borderless_window_proc);
}

void l2dcat_windows_borderless_uninstall(HWND window) {
    if (!window) return;
    WNDPROC original = (WNDPROC)GetPropW(window, original_proc_property);
    if (!original) return;
    SetWindowLongPtrW(window, GWLP_WNDPROC, (LONG_PTR)original);
    RemovePropW(window, original_proc_property);
}
#endif
