#include "windows_keys.h"

#ifdef _WIN32
#include <stdio.h>

const char *bongo_windows_key_name(const KBDLLHOOKSTRUCT *key, char output[16]) {
    DWORD code = key->vkCode;
    if (code >= 'A' && code <= 'Z') {
        snprintf(output, 16, "Key%c", (char)code);
        return output;
    }
    if (code >= '0' && code <= '9') {
        snprintf(output, 16, "Num%c", (char)code);
        return output;
    }
    if (code >= VK_F1 && code <= VK_F24) {
        snprintf(output, 16, "F%lu", code - VK_F1 + 1);
        return output;
    }
    switch (code) {
    case VK_ESCAPE: return "Escape";
    case VK_TAB: return "Tab";
    case VK_CAPITAL: return "CapsLock";
    case VK_SPACE: return "Space";
    case VK_BACK: return "Backspace";
    case VK_DELETE: return "Delete";
    case VK_INSERT: return "Insert";
    case VK_HOME: return "Home";
    case VK_END: return "End";
    case VK_PRIOR: return "PageUp";
    case VK_NEXT: return "PageDown";
    case VK_UP: return "UpArrow";
    case VK_DOWN: return "DownArrow";
    case VK_LEFT: return "LeftArrow";
    case VK_RIGHT: return "RightArrow";
    case VK_LWIN: case VK_RWIN: return "Meta";
    case VK_LSHIFT: return "ShiftLeft";
    case VK_RSHIFT: return "ShiftRight";
    case VK_LCONTROL: return "ControlLeft";
    case VK_RCONTROL: return "ControlRight";
    case VK_LMENU: return "Alt";
    case VK_RMENU: return "AltGr";
    case VK_SHIFT: return key->scanCode == 0x36 ? "ShiftRight" : "ShiftLeft";
    case VK_CONTROL: return key->flags & LLKHF_EXTENDED ? "ControlRight" : "ControlLeft";
    case VK_MENU: return key->flags & LLKHF_EXTENDED ? "AltGr" : "Alt";
    case VK_RETURN: return "Return";
    case VK_OEM_3: return "BackQuote";
    case VK_OEM_MINUS: return "Minus";
    case VK_OEM_PLUS: return "Equal";
    case VK_OEM_4: return "BracketLeft";
    case VK_OEM_6: return "BracketRight";
    case VK_OEM_5: return "Backslash";
    case VK_OEM_1: return "Semicolon";
    case VK_OEM_7: return "Quote";
    case VK_OEM_COMMA: return "Comma";
    case VK_OEM_PERIOD: return "Period";
    case VK_OEM_2: return "Slash";
    default: return NULL;
    }
}
#endif
