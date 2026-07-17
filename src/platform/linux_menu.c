#include "linux_internal.h"

#if !defined(_WIN32) && !defined(__APPLE__)
#include <SDL3/SDL.h>
#include <stdio.h>

static BongoMenuAction submenu(SDL_Window *window, const char *title,
    const int *values, int count, BongoMenuAction first) {
    SDL_MessageBoxButtonData buttons[7]; char labels[7][16];
    for (int i = 0; i < count; ++i) {
        snprintf(labels[i], sizeof(labels[i]), "%d%%", values[i]);
        buttons[i] = (SDL_MessageBoxButtonData){0, i, labels[i]};
    }
    SDL_MessageBoxData data = {SDL_MESSAGEBOX_INFORMATION, window, title, title,
        count, buttons, NULL};
    int selected = -1;
    return SDL_ShowMessageBox(&data, &selected) && selected >= 0
        ? (BongoMenuAction)(first + selected) : BONGO_MENU_NONE;
}

BongoMenuAction bongo_linux_context_menu(BongoPlatform *platform,
    const BongoMenuLabels *labels) {
    if (!platform || !labels) return BONGO_MENU_NONE;
    char pass[BONGO_ID_CAP], top[BONGO_ID_CAP];
    snprintf(pass, sizeof(pass), "%s%s", labels->pass_through_checked ? "✓ " : "",
        labels->pass_through);
    snprintf(top, sizeof(top), "%s%s", labels->always_on_top_checked ? "✓ " : "",
        labels->always_on_top);
    SDL_MessageBoxButtonData buttons[] = {
        {0, BONGO_MENU_PREFERENCES, labels->preferences}, {0, BONGO_MENU_HIDE, labels->hide},
        {0, BONGO_MENU_PASS_THROUGH, pass}, {0, BONGO_MENU_ALWAYS_ON_TOP, top},
        {0, -1, labels->window_size}, {0, -2, labels->opacity},
        {0, BONGO_MENU_RESTART, labels->restart}, {0, BONGO_MENU_EXIT, labels->exit}};
    SDL_MessageBoxData data = {SDL_MESSAGEBOX_INFORMATION, platform->window,
        BONGO_NAME, BONGO_NAME, 8, buttons, NULL};
    int selected = 0;
    if (!SDL_ShowMessageBox(&data, &selected)) return BONGO_MENU_NONE;
    if (selected == -1) {
        const int values[] = {50,75,100,125,150,200};
        return submenu(platform->window, labels->window_size, values, 6, BONGO_MENU_SCALE_50);
    }
    if (selected == -2) {
        const int values[] = {25,50,75,100};
        return submenu(platform->window, labels->opacity, values, 4, BONGO_MENU_OPACITY_25);
    }
    return (BongoMenuAction)selected;
}
#endif
