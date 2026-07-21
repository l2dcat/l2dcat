#include "test.h"
#include "l2dcat/config.h"
#include "l2dcat/file.h"
#include "l2dcat/path.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

void test_config(void) {
    L2DCatConfig value;
    l2dcat_config_defaults(&value);
    CHECK(value.model.max_fps == 60);
    CHECK(value.window.width == 612);
    CHECK(value.window.height == 354);
    CHECK(value.window.visible && value.window.always_on_top);
    CHECK(value.current_mode == L2DCAT_MODE_GAMEPAD);

    value.model.max_fps = 900;
    value.window.scale_percent = -2.0f;
    value.window.opacity_percent = 123.0f;
    l2dcat_config_validate(&value);
    CHECK(value.model.max_fps == 240);
    CHECK(value.window.scale_percent == 10.0f);
    CHECK(value.window.opacity_percent == 100.0f);

    value.model.max_fps = 30;
    value.window.x = -321;
    value.app.language = L2DCAT_LANG_ZH_CN;
    memcpy(value.current_model, "keyboard", sizeof("keyboard"));
    value.current_mode = L2DCAT_MODE_KEYBOARD;
    value.behavior_shortcut_count = 1;
    memcpy(value.behavior_shortcuts[0].id, "keyboard:motion:Tap:0",
        sizeof("keyboard:motion:Tap:0"));
    memcpy(value.behavior_shortcuts[0].shortcut, "Control+1", sizeof("Control+1"));
    L2DCatError error = {0};
    const char *path = "l2dcat-\xE8\xAE\xBE\xE7\xBD\xAE.json";
    CHECK(l2dcat_config_save(path, &value, &error) == L2DCAT_OK);
    CHECK(l2dcat_path_is_file(path));
    char found[L2DCAT_PATH_CAP];
    CHECK(l2dcat_path_find_suffix(".", "\xE8\xAE\xBE\xE7\xBD\xAE.json",
        found, sizeof(found)));
    CHECK(strcmp(found, path) == 0);

    L2DCatConfig loaded;
    CHECK(l2dcat_config_load(path, &loaded, &error) == L2DCAT_OK);
    CHECK(loaded.model.max_fps == 30);
    CHECK(loaded.window.x == -321);
    CHECK(loaded.app.language == L2DCAT_LANG_ZH_CN);
    CHECK(strcmp(loaded.current_model, "keyboard") == 0);
    CHECK(loaded.current_mode == L2DCAT_MODE_KEYBOARD);
    CHECK(loaded.behavior_shortcut_count == 1);
    CHECK(strcmp(loaded.behavior_shortcuts[0].shortcut, "Control+1") == 0);
    CHECK(l2dcat_file_remove(path));

    const char *legacy_path = "l2dcat-legacy.json";
    FILE *legacy = l2dcat_file_open(legacy_path, "wb");
    CHECK(legacy != NULL);
    if (legacy) {
        fputs("{\"schemaVersion\":1,\"window\":{\"visible\":false,"
            "\"alwaysOnTop\":false}}", legacy);
        fclose(legacy);
    }
    CHECK(l2dcat_config_load(legacy_path, &loaded, &error) == L2DCAT_OK);
    CHECK(loaded.schema_version == 2);
    CHECK(loaded.window.visible && loaded.window.always_on_top);
    CHECK(l2dcat_file_remove(legacy_path));
}
