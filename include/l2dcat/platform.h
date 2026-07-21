#ifndef L2DCAT_PLATFORM_H
#define L2DCAT_PLATFORM_H

#include "l2dcat/config.h"
#include "l2dcat/input.h"

typedef struct SDL_Window SDL_Window;

typedef struct L2DCatPlatform {
    SDL_Window *window;
    L2DCatInputState *input;
    void *native;
} L2DCatPlatform;

typedef enum L2DCatMenuAction {
    L2DCAT_MENU_NONE,
    L2DCAT_MENU_PREFERENCES,
    L2DCAT_MENU_HIDE,
    L2DCAT_MENU_PASS_THROUGH,
    L2DCAT_MENU_ALWAYS_ON_TOP,
    L2DCAT_MENU_SCALE_50,
    L2DCAT_MENU_SCALE_75,
    L2DCAT_MENU_SCALE_100,
    L2DCAT_MENU_SCALE_125,
    L2DCAT_MENU_SCALE_150,
    L2DCAT_MENU_SCALE_200,
    L2DCAT_MENU_OPACITY_25,
    L2DCAT_MENU_OPACITY_50,
    L2DCAT_MENU_OPACITY_75,
    L2DCAT_MENU_OPACITY_100,
    L2DCAT_MENU_RESTART,
    L2DCAT_MENU_EXIT,
    L2DCAT_MENU_MODEL_FIRST = 1000
} L2DCatMenuAction;
typedef void (*L2DCatMenuPreview)(void *userdata, L2DCatMenuAction action);

typedef struct L2DCatMenuLabels {
    const char *preferences, *hide, *pass_through, *always_on_top;
    const char *window_size, *opacity, *model, *restart, *exit;
    const char *const *model_names;
    size_t model_count, current_model;
    float scale_percent, opacity_percent;
    bool pass_through_checked, always_on_top_checked;
    L2DCatMenuPreview preview, restore;
    void *preview_userdata;
} L2DCatMenuLabels;

typedef void (*L2DCatDownloadProgress)(uint64_t received, uint64_t total,
    void *userdata);

L2DCatResult l2dcat_platform_init(L2DCatPlatform *platform, SDL_Window *window,
    L2DCatInputState *input, L2DCatError *error);
void l2dcat_platform_shutdown(L2DCatPlatform *platform);
void l2dcat_platform_set_click_through(L2DCatPlatform *platform, bool enabled);
bool l2dcat_platform_pointer_local(L2DCatPlatform *platform, double screen_x,
    double screen_y, float *local_x, float *local_y);
void l2dcat_platform_set_always_on_top(L2DCatPlatform *platform, bool enabled);
void l2dcat_platform_set_taskbar(L2DCatPlatform *platform, bool visible);
void l2dcat_platform_raise_window(SDL_Window *window);
bool l2dcat_platform_set_geometry(L2DCatPlatform *platform,
    int x, int y, int width, int height);
void l2dcat_platform_begin_drag(L2DCatPlatform *platform);
bool l2dcat_platform_global_input_supported(void);
bool l2dcat_platform_single_instance_begin(void);
void l2dcat_platform_single_instance_end(void);
L2DCatResult l2dcat_platform_set_autostart(bool enabled, L2DCatError *error);
bool l2dcat_platform_is_elevated(void);
L2DCatMenuAction l2dcat_platform_context_menu(L2DCatPlatform *platform,
    const L2DCatMenuLabels *labels);
bool l2dcat_platform_restart(void);
L2DCatResult l2dcat_platform_embedded_assets(const char *target, L2DCatError *error);
L2DCatResult l2dcat_platform_download(const char *url, const char *destination,
    uint64_t limit, L2DCatDownloadProgress progress, void *userdata, L2DCatError *error);
bool l2dcat_platform_schedule_update(const char *staged, L2DCatError *error);
bool l2dcat_platform_verify_update(const char *path, const char *version,
    const char *platform, const char *sha256, uint64_t size,
    const char *signature, L2DCatError *error);
int l2dcat_platform_update_helper(int argc, char **argv);

#endif
