#ifndef L2DCAT_CONFIG_H
#define L2DCAT_CONFIG_H

#include "l2dcat/common.h"

typedef enum L2DCatTheme { L2DCAT_THEME_AUTO, L2DCAT_THEME_LIGHT, L2DCAT_THEME_DARK } L2DCatTheme;
typedef enum L2DCatLanguage {
    L2DCAT_LANG_EN_US,
    L2DCAT_LANG_ZH_CN,
    L2DCAT_LANG_ZH_TW,
    L2DCAT_LANG_PT_BR,
    L2DCAT_LANG_VI_VN
} L2DCatLanguage;
typedef enum L2DCatModelMode {
    L2DCAT_MODE_STANDARD,
    L2DCAT_MODE_KEYBOARD,
    L2DCAT_MODE_GAMEPAD
} L2DCatModelMode;

typedef struct L2DCatModelOptions {
    bool mirror;
    bool mouse_mirror;
    bool motion_sound;
    bool behavior;
    bool ignore_mouse;
    float auto_release_seconds;
    int max_fps;
} L2DCatModelOptions;

typedef struct L2DCatWindowOptions {
    bool visible;
    bool pass_through;
    bool always_on_top;
    bool taskbar_visible;
    bool hide_on_hover;
    bool keep_in_screen;
    float scale_percent;
    float opacity_percent;
    float radius_percent;
    float hide_delay_seconds;
    int x;
    int y;
    int width;
    int height;
} L2DCatWindowOptions;

typedef struct L2DCatAppOptions {
    bool autostart;
    bool tray_visible;
    L2DCatTheme theme;
    L2DCatLanguage language;
} L2DCatAppOptions;

typedef struct L2DCatShortcutOptions {
    char visible_cat[L2DCAT_SHORTCUT_CAP];
    char visible_preferences[L2DCAT_SHORTCUT_CAP];
    char mirror[L2DCAT_SHORTCUT_CAP];
    char pass_through[L2DCAT_SHORTCUT_CAP];
    char always_on_top[L2DCAT_SHORTCUT_CAP];
} L2DCatShortcutOptions;

typedef struct L2DCatBehaviorShortcut {
    char id[L2DCAT_PATH_CAP];
    char shortcut[L2DCAT_SHORTCUT_CAP];
} L2DCatBehaviorShortcut;

typedef struct L2DCatConfig {
    uint32_t schema_version;
    L2DCatModelOptions model;
    L2DCatWindowOptions window;
    L2DCatAppOptions app;
    L2DCatShortcutOptions shortcuts;
    L2DCatBehaviorShortcut behavior_shortcuts[L2DCAT_BEHAVIOR_CAP];
    size_t behavior_shortcut_count;
    char current_model[L2DCAT_PATH_CAP];
    L2DCatModelMode current_mode;
} L2DCatConfig;

#ifdef __cplusplus
extern "C" {
#endif

void l2dcat_config_defaults(L2DCatConfig *config);
void l2dcat_config_validate(L2DCatConfig *config);
L2DCatResult l2dcat_config_load(const char *path, L2DCatConfig *config, L2DCatError *error);
L2DCatResult l2dcat_config_save(const char *path, const L2DCatConfig *config, L2DCatError *error);
const char *l2dcat_theme_name(L2DCatTheme value);
const char *l2dcat_language_name(L2DCatLanguage value);
const char *l2dcat_mode_name(L2DCatModelMode value);

#ifdef __cplusplus
}
#endif

#endif
