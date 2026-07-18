#ifndef L2DCAT_WINDOWS_KEYS_H
#define L2DCAT_WINDOWS_KEYS_H

#ifdef _WIN32
#include <windows.h>
const char *l2dcat_windows_key_name(const KBDLLHOOKSTRUCT *key, char output[16]);
#endif

#endif
