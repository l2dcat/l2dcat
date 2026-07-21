#include "linux_internal.h"

#if !defined(_WIN32) && !defined(__APPLE__)
#include <SDL3/SDL.h>
#include <stdio.h>

static L2DCatMenuAction submenu(SDL_Window *window, const char *title,
    const int *values, int count, L2DCatMenuAction first) {
    SDL_MessageBoxButtonData buttons[10]; char labels[10][16];
    for (int i = 0; i < count; ++i) {
        snprintf(labels[i], sizeof(labels[i]), "%d%%", values[i]);
        buttons[i] = (SDL_MessageBoxButtonData){0, i, labels[i]};
    }
    SDL_MessageBoxData data = {SDL_MESSAGEBOX_INFORMATION, window, title, title,
        count, buttons, NULL};
    int selected = -1;
    return SDL_ShowMessageBox(&data, &selected) && selected >= 0
        ? (L2DCatMenuAction)(first + selected) : L2DCAT_MENU_NONE;
}

L2DCatMenuAction l2dcat_linux_context_menu(L2DCatPlatform *platform,
    const L2DCatMenuLabels *labels) {
    if (!platform || !labels) return L2DCAT_MENU_NONE;
    char pass[L2DCAT_ID_CAP], top[L2DCAT_ID_CAP];
    snprintf(pass, sizeof(pass), "%s%s", labels->pass_through_checked ? "✓ " : "",
        labels->pass_through);
    snprintf(top, sizeof(top), "%s%s", labels->always_on_top_checked ? "✓ " : "",
        labels->always_on_top);
    SDL_MessageBoxButtonData buttons[] = {
        {0, L2DCAT_MENU_PREFERENCES, labels->preferences}, {0, L2DCAT_MENU_HIDE, labels->hide},
        {0, L2DCAT_MENU_PASS_THROUGH, pass}, {0, L2DCAT_MENU_ALWAYS_ON_TOP, top},
        {0, -1, labels->window_size}, {0, -2, labels->opacity}, {0, -3, labels->model},
        {0, L2DCAT_MENU_EXIT, labels->exit}};
    SDL_MessageBoxData data = {SDL_MESSAGEBOX_INFORMATION, platform->window,
        L2DCAT_NAME, L2DCAT_NAME, 8, buttons, NULL};
    int selected = 0;
    if (!SDL_ShowMessageBox(&data, &selected)) return L2DCAT_MENU_NONE;
    if (selected == -1) {
        const int values[] = {50,75,100,125,150,200};
        return submenu(platform->window, labels->window_size, values, 6, L2DCAT_MENU_SCALE_50);
    }
    if (selected == -2) {
        const int values[] = {10,20,30,40,50,60,70,80,90,100};
        return submenu(platform->window, labels->opacity, values, 10, L2DCAT_MENU_OPACITY_10);
    }
    if (selected == -3 && labels->model_count) {
        SDL_MessageBoxButtonData model_buttons[L2DCAT_MODEL_CAP];
        for (size_t i = 0; i < labels->model_count; ++i)
            model_buttons[i] = (SDL_MessageBoxButtonData){0,
                L2DCAT_MENU_MODEL_FIRST + (int)i, labels->model_names[i]};
        SDL_MessageBoxData models = {SDL_MESSAGEBOX_INFORMATION, platform->window,
            labels->model, labels->model, (int)labels->model_count, model_buttons, NULL};
        int model = -1;
        return SDL_ShowMessageBox(&models, &model) && model >= 0
            ? (L2DCatMenuAction)model : L2DCAT_MENU_NONE;
    }
    return (L2DCatMenuAction)selected;
}
#endif
