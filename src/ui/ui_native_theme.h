#ifndef L2DCAT_UI_NATIVE_THEME_H
#define L2DCAT_UI_NATIVE_THEME_H

#include <stdbool.h>

typedef struct SDL_Window SDL_Window;

void l2dcat_ui_native_theme_apply(SDL_Window *window, bool dark);
void l2dcat_ui_native_menu_prepare(SDL_Window *window, bool dark);

#endif
