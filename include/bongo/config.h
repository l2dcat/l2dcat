#ifndef BONGO_CONFIG_H
#define BONGO_CONFIG_H

#include "bongo/common.h"

typedef enum BongoTheme { BONGO_THEME_AUTO, BONGO_THEME_LIGHT, BONGO_THEME_DARK } BongoTheme;
typedef enum BongoLanguage {
    BONGO_LANG_EN_US,
    BONGO_LANG_ZH_CN,
    BONGO_LANG_ZH_TW,
    BONGO_LANG_PT_BR,
    BONGO_LANG_VI_VN
} BongoLanguage;
typedef enum BongoModelMode {
    BONGO_MODE_STANDARD,
    BONGO_MODE_KEYBOARD,
    BONGO_MODE_GAMEPAD
} BongoModelMode;

typedef struct BongoModelOptions {
    bool mirror;
    bool mouse_mirror;
    bool motion_sound;
    bool behavior;
    bool ignore_mouse;
    float auto_release_seconds;
    int max_fps;
} BongoModelOptions;

typedef struct BongoWindowOptions {
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
} BongoWindowOptions;

typedef struct BongoAppOptions {
    bool autostart;
    bool tray_visible;
    BongoTheme theme;
    BongoLanguage language;
} BongoAppOptions;

typedef struct BongoShortcutOptions {
    char visible_cat[BONGO_SHORTCUT_CAP];
    char visible_preferences[BONGO_SHORTCUT_CAP];
    char mirror[BONGO_SHORTCUT_CAP];
    char pass_through[BONGO_SHORTCUT_CAP];
    char always_on_top[BONGO_SHORTCUT_CAP];
} BongoShortcutOptions;

typedef struct BongoBehaviorShortcut {
    char id[BONGO_PATH_CAP];
    char shortcut[BONGO_SHORTCUT_CAP];
} BongoBehaviorShortcut;

typedef struct BongoConfig {
    uint32_t schema_version;
    BongoModelOptions model;
    BongoWindowOptions window;
    BongoAppOptions app;
    BongoShortcutOptions shortcuts;
    BongoBehaviorShortcut behavior_shortcuts[BONGO_BEHAVIOR_CAP];
    size_t behavior_shortcut_count;
    char current_model[BONGO_PATH_CAP];
    BongoModelMode current_mode;
} BongoConfig;

void bongo_config_defaults(BongoConfig *config);
void bongo_config_validate(BongoConfig *config);
BongoResult bongo_config_load(const char *path, BongoConfig *config, BongoError *error);
BongoResult bongo_config_save(const char *path, const BongoConfig *config, BongoError *error);
const char *bongo_theme_name(BongoTheme value);
const char *bongo_language_name(BongoLanguage value);
const char *bongo_mode_name(BongoModelMode value);

#endif
