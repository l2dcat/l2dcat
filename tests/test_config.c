#include "test.h"
#include "bongo/config.h"
#include "bongo/file.h"
#include "bongo/path.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

void test_config(void) {
    BongoConfig value;
    bongo_config_defaults(&value);
    CHECK(value.model.max_fps == 60);
    CHECK(value.window.width == 612);
    CHECK(value.window.height == 354);
    CHECK(value.current_mode == BONGO_MODE_GAMEPAD);

    value.model.max_fps = 900;
    value.window.scale_percent = -2.0f;
    value.window.opacity_percent = 123.0f;
    bongo_config_validate(&value);
    CHECK(value.model.max_fps == 240);
    CHECK(value.window.scale_percent == 10.0f);
    CHECK(value.window.opacity_percent == 100.0f);

    value.model.max_fps = 30;
    value.window.x = -321;
    value.app.language = BONGO_LANG_ZH_CN;
    strcpy(value.current_model, "keyboard");
    value.current_mode = BONGO_MODE_KEYBOARD;
    value.behavior_shortcut_count = 1;
    strcpy(value.behavior_shortcuts[0].id, "keyboard:motion:Tap:0");
    strcpy(value.behavior_shortcuts[0].shortcut, "Control+1");
    BongoError error = {0};
    const char *path = "bongo-\xE8\xAE\xBE\xE7\xBD\xAE.json";
    CHECK(bongo_config_save(path, &value, &error) == BONGO_OK);
    CHECK(bongo_path_is_file(path));
    char found[BONGO_PATH_CAP];
    CHECK(bongo_path_find_suffix(".", "\xE8\xAE\xBE\xE7\xBD\xAE.json",
        found, sizeof(found)));
    CHECK(strcmp(found, path) == 0);

    BongoConfig loaded;
    CHECK(bongo_config_load(path, &loaded, &error) == BONGO_OK);
    CHECK(loaded.model.max_fps == 30);
    CHECK(loaded.window.x == -321);
    CHECK(loaded.app.language == BONGO_LANG_ZH_CN);
    CHECK(strcmp(loaded.current_model, "keyboard") == 0);
    CHECK(loaded.current_mode == BONGO_MODE_KEYBOARD);
    CHECK(loaded.behavior_shortcut_count == 1);
    CHECK(strcmp(loaded.behavior_shortcuts[0].shortcut, "Control+1") == 0);
    CHECK(bongo_file_remove(path));
}
