#ifndef L2DCAT_WINDOWS_BORDERLESS_H
#define L2DCAT_WINDOWS_BORDERLESS_H

#ifdef _WIN32
#include "l2dcat/platform.h"
#include <windows.h>

void l2dcat_windows_borderless_install(HWND window);
void l2dcat_windows_borderless_uninstall(HWND window);
void l2dcat_windows_menu_preview(L2DCatMenuPreview preview, void *userdata);
#endif

#endif
