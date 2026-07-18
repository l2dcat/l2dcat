#ifndef L2DCAT_WINDOWS_UTF8_H
#define L2DCAT_WINDOWS_UTF8_H

#ifdef _WIN32
#include <stdbool.h>
#include <stddef.h>
#include <wchar.h>

wchar_t *l2dcat_windows_wide(const char *text);
bool l2dcat_windows_utf8(const wchar_t *text, char *output, size_t capacity);
#endif

#endif
