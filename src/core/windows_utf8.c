#include "windows_utf8.h"

#ifdef _WIN32
#include <stdlib.h>
#include <windows.h>

wchar_t *bongo_windows_wide(const char *text) {
    int length = text ? MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
        text, -1, NULL, 0) : 0;
    wchar_t *wide = length > 0 ? malloc((size_t)length * sizeof(*wide)) : NULL;
    if (wide && !MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
        text, -1, wide, length)) {
        free(wide); return NULL;
    }
    return wide;
}

bool bongo_windows_utf8(const wchar_t *text, char *output, size_t capacity) {
    if (!text || !output || !capacity) return false;
    int required = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
        text, -1, NULL, 0, NULL, NULL);
    return required > 0 && (size_t)required <= capacity &&
        WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
            text, -1, output, (int)capacity, NULL, NULL) > 0;
}
#endif
