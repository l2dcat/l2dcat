#ifndef BONGO_PLATFORM_H
#define BONGO_PLATFORM_H

#include "bongo/config.h"
#include "bongo/input.h"

typedef struct SDL_Window SDL_Window;

typedef struct BongoPlatform {
    SDL_Window *window;
    BongoInputState *input;
    void *native;
} BongoPlatform;

typedef enum BongoMenuAction {
    BONGO_MENU_NONE,
    BONGO_MENU_PREFERENCES,
    BONGO_MENU_HIDE,
    BONGO_MENU_PASS_THROUGH,
    BONGO_MENU_ALWAYS_ON_TOP,
    BONGO_MENU_SCALE_50,
    BONGO_MENU_SCALE_75,
    BONGO_MENU_SCALE_100,
    BONGO_MENU_SCALE_125,
    BONGO_MENU_SCALE_150,
    BONGO_MENU_SCALE_200,
    BONGO_MENU_OPACITY_25,
    BONGO_MENU_OPACITY_50,
    BONGO_MENU_OPACITY_75,
    BONGO_MENU_OPACITY_100,
    BONGO_MENU_RESTART,
    BONGO_MENU_EXIT
} BongoMenuAction;

typedef struct BongoMenuLabels {
    const char *preferences, *hide, *pass_through, *always_on_top;
    const char *window_size, *opacity, *restart, *exit;
    bool pass_through_checked, always_on_top_checked;
} BongoMenuLabels;

typedef void (*BongoDownloadProgress)(uint64_t received, uint64_t total,
    void *userdata);

BongoResult bongo_platform_init(BongoPlatform *platform, SDL_Window *window,
    BongoInputState *input, BongoError *error);
void bongo_platform_shutdown(BongoPlatform *platform);
void bongo_platform_set_click_through(BongoPlatform *platform, bool enabled);
void bongo_platform_set_always_on_top(BongoPlatform *platform, bool enabled);
void bongo_platform_set_taskbar(BongoPlatform *platform, bool visible);
void bongo_platform_begin_drag(BongoPlatform *platform);
bool bongo_platform_global_input_supported(void);
bool bongo_platform_single_instance_begin(void);
void bongo_platform_single_instance_end(void);
BongoResult bongo_platform_set_autostart(bool enabled, BongoError *error);
bool bongo_platform_is_elevated(void);
BongoMenuAction bongo_platform_context_menu(BongoPlatform *platform,
    const BongoMenuLabels *labels);
bool bongo_platform_restart(void);
BongoResult bongo_platform_embedded_assets(const char *target, BongoError *error);
BongoResult bongo_platform_download(const char *url, const char *destination,
    uint64_t limit, BongoDownloadProgress progress, void *userdata, BongoError *error);
bool bongo_platform_schedule_update(const char *staged, BongoError *error);
bool bongo_platform_verify_update(const char *path, const char *version,
    const char *platform, const char *sha256, uint64_t size,
    const char *signature, BongoError *error);
int bongo_platform_update_helper(int argc, char **argv);

#endif
