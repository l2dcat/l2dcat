#ifndef BONGO_WINDOWS_KEYS_H
#define BONGO_WINDOWS_KEYS_H

#ifdef _WIN32
#include <windows.h>
const char *bongo_windows_key_name(const KBDLLHOOKSTRUCT *key, char output[16]);
#endif

#endif
