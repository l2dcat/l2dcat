#include "l2dcat/config.h"
#include "l2dcat/file.h"
#include "l2dcat/path.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <yyjson.h>

#ifdef _WIN32
#include "windows_utf8.h"
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

static bool get_bool(yyjson_val *obj, const char *key, bool fallback) {
    yyjson_val *value = yyjson_obj_get(obj, key);
    return yyjson_is_bool(value) ? yyjson_get_bool(value) : fallback;
}

static int get_int(yyjson_val *obj, const char *key, int fallback) {
    yyjson_val *value = yyjson_obj_get(obj, key);
    return yyjson_is_num(value) ? (int)yyjson_get_sint(value) : fallback;
}

static float get_float(yyjson_val *obj, const char *key, float fallback) {
    yyjson_val *value = yyjson_obj_get(obj, key);
    return yyjson_is_num(value) ? (float)yyjson_get_num(value) : fallback;
}

static const char *get_string(yyjson_val *obj, const char *key) {
    yyjson_val *value = yyjson_obj_get(obj, key);
    return yyjson_is_str(value) ? yyjson_get_str(value) : NULL;
}

static L2DCatTheme parse_theme(const char *value) {
    if (value && strcmp(value, "light") == 0) return L2DCAT_THEME_LIGHT;
    if (value && strcmp(value, "dark") == 0) return L2DCAT_THEME_DARK;
    return L2DCAT_THEME_AUTO;
}

static L2DCatLanguage parse_language(const char *value) {
    const char *names[] = {"en-US", "zh-CN", "zh-TW", "pt-BR", "vi-VN"};
    if (value) for (int i = 0; i <= L2DCAT_LANG_VI_VN; ++i) {
        if (strcmp(value, names[i]) == 0) return (L2DCatLanguage)i;
    }
    return L2DCAT_LANG_EN_US;
}

static L2DCatModelMode parse_mode(const char *value) {
    if (value && strcmp(value, "keyboard") == 0) return L2DCAT_MODE_KEYBOARD;
    if (value && strcmp(value, "gamepad") == 0) return L2DCAT_MODE_GAMEPAD;
    return L2DCAT_MODE_STANDARD;
}

static void read_model(yyjson_val *obj, L2DCatModelOptions *value) {
    if (!yyjson_is_obj(obj)) return;
    value->mirror = get_bool(obj, "mirror", value->mirror);
    value->mouse_mirror = get_bool(obj, "mouseMirror", value->mouse_mirror);
    value->motion_sound = get_bool(obj, "motionSound", value->motion_sound);
    value->behavior = get_bool(obj, "behavior", value->behavior);
    value->ignore_mouse = get_bool(obj, "ignoreMouse", value->ignore_mouse);
    value->auto_release_seconds = get_float(obj, "autoReleaseDelay", value->auto_release_seconds);
    value->max_fps = get_int(obj, "maxFPS", value->max_fps);
}

static void read_window(yyjson_val *obj, L2DCatWindowOptions *value) {
    if (!yyjson_is_obj(obj)) return;
    value->visible = get_bool(obj, "visible", value->visible);
    value->pass_through = get_bool(obj, "passThrough", value->pass_through);
    value->always_on_top = get_bool(obj, "alwaysOnTop", value->always_on_top);
    value->taskbar_visible = get_bool(obj, "taskbarVisible", value->taskbar_visible);
    value->hide_on_hover = get_bool(obj, "hideOnHover", value->hide_on_hover);
    value->keep_in_screen = get_bool(obj, "keepInScreen", value->keep_in_screen);
    value->scale_percent = get_float(obj, "scale", value->scale_percent);
    value->opacity_percent = get_float(obj, "opacity", value->opacity_percent);
    value->radius_percent = get_float(obj, "radius", value->radius_percent);
    value->hide_delay_seconds = get_float(obj, "hideOnHoverDelay", value->hide_delay_seconds);
    value->x = get_int(obj, "x", value->x);
    value->y = get_int(obj, "y", value->y);
    value->width = get_int(obj, "width", value->width);
    value->height = get_int(obj, "height", value->height);
}

static void read_app(yyjson_val *obj, L2DCatAppOptions *value) {
    if (!yyjson_is_obj(obj)) return;
    value->autostart = get_bool(obj, "autostart", value->autostart);
    value->tray_visible = get_bool(obj, "trayVisible", value->tray_visible);
    value->theme = parse_theme(get_string(obj, "theme"));
    value->language = parse_language(get_string(obj, "language"));
}

static void copy_string(char *target, size_t capacity, const char *value) {
    if (value) snprintf(target, capacity, "%s", value);
}

static void read_shortcuts(yyjson_val *obj, L2DCatShortcutOptions *value) {
    if (!yyjson_is_obj(obj)) return;
    copy_string(value->visible_cat, sizeof(value->visible_cat), get_string(obj, "visibleCat"));
    copy_string(value->visible_preferences, sizeof(value->visible_preferences),
        get_string(obj, "visiblePreference"));
    copy_string(value->mirror, sizeof(value->mirror), get_string(obj, "mirrorMode"));
    copy_string(value->pass_through, sizeof(value->pass_through), get_string(obj, "penetrable"));
    copy_string(value->always_on_top, sizeof(value->always_on_top), get_string(obj, "alwaysOnTop"));
}

static void read_behaviors(yyjson_val *array, L2DCatConfig *config) {
    if (!yyjson_is_arr(array)) return;
    size_t index, count; yyjson_val *item;
    yyjson_arr_foreach(array, index, count, item) {
        const char *id = get_string(item, "id");
        const char *shortcut = get_string(item, "shortcut");
        if (!id || !shortcut || config->behavior_shortcut_count >= L2DCAT_BEHAVIOR_CAP) continue;
        L2DCatBehaviorShortcut *value =
            &config->behavior_shortcuts[config->behavior_shortcut_count++];
        copy_string(value->id, sizeof(value->id), id);
        copy_string(value->shortcut, sizeof(value->shortcut), shortcut);
    }
}

L2DCatResult l2dcat_config_load(const char *path, L2DCatConfig *config, L2DCatError *error) {
    if (!path || !config) return L2DCAT_ERROR_ARGUMENT;
    l2dcat_config_defaults(config);
    if (!l2dcat_path_is_file(path)) return L2DCAT_OK;
    yyjson_read_err json_error = {0};
    FILE *file = l2dcat_file_open(path, "rb");
    yyjson_doc *doc = file ? yyjson_read_fp(file, 0, NULL, &json_error) : NULL;
    if (file) fclose(file);
    if (!doc) {
        l2dcat_error_set(error, L2DCAT_ERROR_FORMAT, "Invalid settings JSON: %s",
            json_error.msg ? json_error.msg : "cannot open file");
        return L2DCAT_ERROR_FORMAT;
    }
    yyjson_val *root = yyjson_doc_get_root(doc);
    uint32_t loaded_schema = (uint32_t)get_int(root, "schemaVersion", 1);
    config->schema_version = loaded_schema;
    read_model(yyjson_obj_get(root, "model"), &config->model);
    read_window(yyjson_obj_get(root, "window"), &config->window);
    read_app(yyjson_obj_get(root, "app"), &config->app);
    read_shortcuts(yyjson_obj_get(root, "shortcuts"), &config->shortcuts);
    read_behaviors(yyjson_obj_get(root, "behaviorShortcuts"), config);
    const char *model = get_string(root, "currentModel");
    if (model) snprintf(config->current_model, sizeof(config->current_model), "%s", model);
    config->current_mode = parse_mode(get_string(root, "currentMode"));
    if (loaded_schema < 2) {
        config->window.visible = true;
        config->window.always_on_top = true;
    }
    yyjson_doc_free(doc);
    l2dcat_config_validate(config);
    return L2DCAT_OK;
}

static void write_model(yyjson_mut_doc *doc, yyjson_mut_val *obj, const L2DCatModelOptions *v) {
    yyjson_mut_obj_add_bool(doc, obj, "mirror", v->mirror);
    yyjson_mut_obj_add_bool(doc, obj, "mouseMirror", v->mouse_mirror);
    yyjson_mut_obj_add_bool(doc, obj, "motionSound", v->motion_sound);
    yyjson_mut_obj_add_bool(doc, obj, "behavior", v->behavior);
    yyjson_mut_obj_add_bool(doc, obj, "ignoreMouse", v->ignore_mouse);
    yyjson_mut_obj_add_real(doc, obj, "autoReleaseDelay", v->auto_release_seconds);
    yyjson_mut_obj_add_int(doc, obj, "maxFPS", v->max_fps);
}

static void write_window(yyjson_mut_doc *doc, yyjson_mut_val *obj, const L2DCatWindowOptions *v) {
    yyjson_mut_obj_add_bool(doc, obj, "visible", v->visible);
    yyjson_mut_obj_add_bool(doc, obj, "passThrough", v->pass_through);
    yyjson_mut_obj_add_bool(doc, obj, "alwaysOnTop", v->always_on_top);
    yyjson_mut_obj_add_bool(doc, obj, "taskbarVisible", v->taskbar_visible);
    yyjson_mut_obj_add_bool(doc, obj, "hideOnHover", v->hide_on_hover);
    yyjson_mut_obj_add_bool(doc, obj, "keepInScreen", v->keep_in_screen);
    yyjson_mut_obj_add_real(doc, obj, "scale", v->scale_percent);
    yyjson_mut_obj_add_real(doc, obj, "opacity", v->opacity_percent);
    yyjson_mut_obj_add_real(doc, obj, "radius", v->radius_percent);
    yyjson_mut_obj_add_real(doc, obj, "hideOnHoverDelay", v->hide_delay_seconds);
    yyjson_mut_obj_add_int(doc, obj, "x", v->x);
    yyjson_mut_obj_add_int(doc, obj, "y", v->y);
    yyjson_mut_obj_add_int(doc, obj, "width", v->width);
    yyjson_mut_obj_add_int(doc, obj, "height", v->height);
}

static void write_app(yyjson_mut_doc *doc, yyjson_mut_val *obj, const L2DCatAppOptions *v) {
    yyjson_mut_obj_add_bool(doc, obj, "autostart", v->autostart);
    yyjson_mut_obj_add_bool(doc, obj, "trayVisible", v->tray_visible);
    yyjson_mut_obj_add_strcpy(doc, obj, "theme", l2dcat_theme_name(v->theme));
    yyjson_mut_obj_add_strcpy(doc, obj, "language", l2dcat_language_name(v->language));
}

static void write_shortcuts(yyjson_mut_doc *doc, yyjson_mut_val *obj,
    const L2DCatShortcutOptions *value) {
    yyjson_mut_obj_add_strcpy(doc, obj, "visibleCat", value->visible_cat);
    yyjson_mut_obj_add_strcpy(doc, obj, "visiblePreference", value->visible_preferences);
    yyjson_mut_obj_add_strcpy(doc, obj, "mirrorMode", value->mirror);
    yyjson_mut_obj_add_strcpy(doc, obj, "penetrable", value->pass_through);
    yyjson_mut_obj_add_strcpy(doc, obj, "alwaysOnTop", value->always_on_top);
}

static void write_behaviors(yyjson_mut_doc *doc, yyjson_mut_val *root,
    const L2DCatConfig *config) {
    yyjson_mut_val *array = yyjson_mut_arr(doc);
    yyjson_mut_obj_add_val(doc, root, "behaviorShortcuts", array);
    for (size_t i = 0; i < config->behavior_shortcut_count; ++i) {
        const L2DCatBehaviorShortcut *value = &config->behavior_shortcuts[i];
        if (!value->id[0] || !value->shortcut[0]) continue;
        yyjson_mut_val *item = yyjson_mut_obj(doc);
        yyjson_mut_obj_add_strcpy(doc, item, "id", value->id);
        yyjson_mut_obj_add_strcpy(doc, item, "shortcut", value->shortcut);
        yyjson_mut_arr_add_val(array, item);
    }
}

static bool sync_file(const char *path) {
#ifdef _WIN32
    wchar_t *wide = l2dcat_windows_wide(path);
    HANDLE file = wide ? CreateFileW(wide, GENERIC_WRITE, FILE_SHARE_READ, NULL,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL) : INVALID_HANDLE_VALUE;
    free(wide);
    if (file == INVALID_HANDLE_VALUE) return false;
    bool result = FlushFileBuffers(file) != FALSE;
    CloseHandle(file);
    return result;
#else
    int file = open(path, O_RDONLY);
    if (file < 0) return false;
    bool result = fsync(file) == 0;
    close(file);
    return result;
#endif
}

#ifndef _WIN32
static bool sync_parent(const char *path) {
    char directory[L2DCAT_PATH_CAP];
    int length = snprintf(directory, sizeof(directory), "%s", path);
    if (length < 0 || (size_t)length >= sizeof(directory)) return false;
    char *slash = strrchr(directory, '/');
    if (!slash) snprintf(directory, sizeof(directory), ".");
    else if (slash == directory) slash[1] = '\0';
    else *slash = '\0';
    int handle = open(directory, O_RDONLY);
    if (handle < 0) return false;
    bool result = fsync(handle) == 0;
    close(handle);
    return result;
}
#endif

L2DCatResult l2dcat_config_save(const char *path, const L2DCatConfig *config, L2DCatError *error) {
    if (!path || !config) return L2DCAT_ERROR_ARGUMENT;
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    if (!doc) return L2DCAT_ERROR_MEMORY;
    yyjson_mut_val *root = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, root);
    yyjson_mut_obj_add_int(doc, root, "schemaVersion", config->schema_version);
    write_model(doc, yyjson_mut_obj_add_obj(doc, root, "model"), &config->model);
    write_window(doc, yyjson_mut_obj_add_obj(doc, root, "window"), &config->window);
    write_app(doc, yyjson_mut_obj_add_obj(doc, root, "app"), &config->app);
    write_shortcuts(doc, yyjson_mut_obj_add_obj(doc, root, "shortcuts"), &config->shortcuts);
    write_behaviors(doc, root, config);
    yyjson_mut_obj_add_strcpy(doc, root, "currentModel", config->current_model);
    yyjson_mut_obj_add_strcpy(doc, root, "currentMode", l2dcat_mode_name(config->current_mode));
    char temporary[L2DCAT_PATH_CAP + 8];
    snprintf(temporary, sizeof(temporary), "%s.tmp", path);
    yyjson_write_err json_error = {0};
    FILE *file = l2dcat_file_open(temporary, "wb");
    bool written = file && yyjson_mut_write_fp(file, doc,
        YYJSON_WRITE_PRETTY, NULL, &json_error);
    if (file && fclose(file) != 0) written = false;
    yyjson_mut_doc_free(doc);
    if (!written) {
        l2dcat_error_set(error, L2DCAT_ERROR_IO, "Cannot write settings: %s",
            json_error.msg ? json_error.msg : "cannot open file");
        return L2DCAT_ERROR_IO;
    }
    if (!sync_file(temporary)) {
        l2dcat_file_remove(temporary);
        l2dcat_error_set(error, L2DCAT_ERROR_IO, "Cannot flush settings file");
        return L2DCAT_ERROR_IO;
    }
    if (!l2dcat_file_replace(temporary, path, true)) {
        l2dcat_file_remove(temporary);
        l2dcat_error_set(error, L2DCAT_ERROR_IO, "Cannot replace settings file");
        return L2DCAT_ERROR_IO;
    }
#ifndef _WIN32
    if (!sync_parent(path)) {
        l2dcat_error_set(error, L2DCAT_ERROR_IO, "Cannot flush settings directory");
        return L2DCAT_ERROR_IO;
    }
#endif
    return L2DCAT_OK;
}
