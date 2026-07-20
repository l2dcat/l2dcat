#ifndef L2DCAT_WINDOWS_BORDERLESS_H
#define L2DCAT_WINDOWS_BORDERLESS_H

#ifdef _WIN32
#include <windows.h>

void l2dcat_windows_borderless_install(HWND window);
void l2dcat_windows_borderless_uninstall(HWND window);
#endif

#endif
