#include "model_import.h"
#include "model_import_legacy_internal.h"
#include "l2dcat/file.h"
#include "l2dcat/path.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <yyjson.h>

typedef L2DCatLegacyKeyNames KeyNames;

static bool parent_path(const char *path, char *parent, size_t capacity) {
    size_t length = path ? strlen(path) : 0;
    while (length && (path[length - 1] == '/' || path[length - 1] == '\\')) length--;
    while (length && path[length - 1] != '/' && path[length - 1] != '\\') length--;
    while (length > 1 && (path[length - 1] == '/' || path[length - 1] == '\\')) length--;
    if (!length || length >= capacity) return false;
    memcpy(parent, path, length);
    parent[length] = '\0';
    return true;
}

static bool find_config(const char *source, char path[L2DCAT_PATH_CAP]) {
    char directory[L2DCAT_PATH_CAP];
    snprintf(directory, sizeof(directory), "%s", source);
    for (int depth = 0; depth < 4; ++depth) {
        if (l2dcat_path_join(path, L2DCAT_PATH_CAP, directory, "config.json") &&
            l2dcat_path_is_file(path)) return true;
        char parent[L2DCAT_PATH_CAP];
        if (!parent_path(directory, parent, sizeof(parent))) break;
        snprintf(directory, sizeof(directory), "%s", parent);
    }
    return false;
}

static bool child_is_dir(const char *root, const char *name) {
    char path[L2DCAT_PATH_CAP];
    return l2dcat_path_join(path, sizeof(path), root, name) && l2dcat_path_is_dir(path);
}

static int modifier_index(int code) {
    return code == 16 ? 0 : code == 17 ? 1 : code == 18 ? 2 : -1;
}

static void count_modifiers(yyjson_val *matrix, size_t counts[3]) {
    size_t row_index, row_count; yyjson_val *row;
    yyjson_arr_foreach(matrix, row_index, row_count, row) {
        size_t key_index, key_count; yyjson_val *key;
        yyjson_arr_foreach(row, key_index, key_count, key) {
            int index = (yyjson_is_int(key) || yyjson_is_uint(key))
                ? modifier_index((int)yyjson_get_int(key)) : -1;
            if (index >= 0) counts[index]++;
        }
    }
}

static KeyNames device_names(int code, size_t occurrence, size_t total) {
    KeyNames names = {0};
    if (code >= 48 && code <= 57) {
        snprintf(names.generated, sizeof(names.generated), "Num%c", (char)code);
    } else if (code >= 65 && code <= 90) {
        snprintf(names.generated, sizeof(names.generated), "Key%c", (char)code);
    } else if (code >= 112 && code <= 123) {
        snprintf(names.generated, sizeof(names.generated), "F%d", code - 111);
    } else if (modifier_index(code) >= 0) {
        static const char *const modifiers[][2] = {
            {"ShiftLeft", "ShiftRight"}, {"ControlLeft", "ControlRight"},
            {"Alt", "AltGr"}
        };
        int index = modifier_index(code);
        if (total > 1) {
            names.items[0] = modifiers[index][occurrence ? 1 : 0];
            names.count = 1;
        } else {
            names.items[0] = modifiers[index][0];
            names.items[1] = modifiers[index][1];
            names.count = 2;
        }
        return names;
    } else {
        static const struct { int code; const char *name; } map[] = {
            {8,"Backspace"},{9,"Tab"},{13,"Return"},{19,"Pause"},{20,"CapsLock"},
            {27,"Escape"},{32,"Space"},{33,"PageUp"},{34,"PageDown"},{35,"End"},
            {36,"Home"},{37,"LeftArrow"},{38,"UpArrow"},{39,"RightArrow"},
            {40,"DownArrow"},{44,"PrintScreen"},{45,"Insert"},{46,"Delete"},
            {91,"Meta"},{92,"Meta"},{93,"Apps"},{96,"Kp0"},{97,"Kp1"},
            {98,"Kp2"},{99,"Kp3"},{100,"Kp4"},{101,"Kp5"},{102,"Kp6"},
            {103,"Kp7"},{104,"Kp8"},{105,"Kp9"},{106,"KpMultiply"},
            {107,"KpPlus"},{109,"KpMinus"},{110,"KpDecimal"},{111,"KpDivide"},
            {144,"NumLock"},{145,"ScrollLock"},{186,"Semicolon"},{187,"Equal"},
            {188,"Comma"},{189,"Minus"},{190,"Period"},{191,"Slash"},
            {192,"BackQuote"},{219,"BracketLeft"},{220,"Backslash"},
            {221,"BracketRight"},{222,"Quote"}
        };
        for (size_t i = 0; i < sizeof(map) / sizeof(map[0]); ++i)
            if (map[i].code == code) { names.items[0] = map[i].name; names.count = 1; break; }
        return names;
    }
    names.count = 1;
    return names;
}

static KeyNames gamepad_names(int code) {
    static const char *map[] = {"South", "East", "West", "North", "LeftTrigger",
        "RightTrigger", "LeftTrigger2", "RightTrigger2", "LeftThumb", "RightThumb",
        "DPadLeft", "DPadRight", "DPadUp", "DPadDown", "Start", "Select"};
    KeyNames names = {0};
    if (code >= 0 && (size_t)code < sizeof(map) / sizeof(map[0])) {
        names.items[0] = map[code]; names.count = 1;
    }
    return names;
}

static bool keyboard_index(yyjson_val *matrix, int code, size_t *result) {
    if (!yyjson_is_arr(matrix)) return false;
    size_t row_index, row_count; yyjson_val *row;
    yyjson_arr_foreach(matrix, row_index, row_count, row) {
        size_t key_index, key_count; yyjson_val *key;
        yyjson_arr_foreach(row, key_index, key_count, key) {
            if ((yyjson_is_int(key) || yyjson_is_uint(key)) &&
                yyjson_get_int(key) == code) {
                *result = row_index;
                return true;
            }
        }
    }
    return false;
}

static bool process_matrix(const L2DCatImportCandidate *candidate, yyjson_val *matrix,
    yyjson_val *before, yyjson_val *after, yyjson_val *keyboard_matrix,
    const char *hand_name, const char *key_group, const char *target,
    L2DCatError *error) {
    if (!yyjson_is_arr(matrix)) return false;
    char hand_dir[L2DCAT_PATH_CAP], keyboard_dir[L2DCAT_PATH_CAP];
    char resources[L2DCAT_PATH_CAP], output_dir[L2DCAT_PATH_CAP];
    if (!l2dcat_path_join(hand_dir, sizeof(hand_dir), candidate->assets, hand_name) ||
        !l2dcat_path_join(keyboard_dir, sizeof(keyboard_dir), candidate->assets, "keyboard") ||
        !l2dcat_path_join(resources, sizeof(resources), target, "resources") ||
        !l2dcat_path_join(output_dir, sizeof(output_dir), resources, key_group) ||
        !SDL_CreateDirectory(output_dir)) return false;
    size_t modifier_total[3] = {0}, modifier_seen[3] = {0};
    count_modifiers(before, modifier_seen);
    memcpy(modifier_total, modifier_seen, sizeof(modifier_total));
    count_modifiers(matrix, modifier_total);
    count_modifiers(after, modifier_total);
    size_t index, count; yyjson_val *keys;
    yyjson_arr_foreach(matrix, index, count, keys) {
        char hand[L2DCAT_PATH_CAP], filename[32];
        snprintf(filename, sizeof(filename), "%zu.png", index);
        if (!l2dcat_path_join(hand, sizeof(hand), hand_dir, filename) ||
            !l2dcat_path_is_file(hand)) {
            size_t missing_index, missing_count; yyjson_val *missing;
            yyjson_arr_foreach(keys, missing_index, missing_count, missing) {
                int modifier = (yyjson_is_int(missing) || yyjson_is_uint(missing))
                    ? modifier_index((int)yyjson_get_int(missing)) : -1;
                if (modifier >= 0) modifier_seen[modifier]++;
            }
            continue;
        }
        size_t key_index, key_count; yyjson_val *key;
        yyjson_arr_foreach(keys, key_index, key_count, key) {
            if (!yyjson_is_int(key) && !yyjson_is_uint(key)) continue;
            int code = (int)yyjson_get_int(key);
            char keyboard[L2DCAT_PATH_CAP];
            size_t keyboard_row;
            const char *keyboard_path = NULL;
            if (keyboard_index(keyboard_matrix, code, &keyboard_row)) {
                snprintf(filename, sizeof(filename), "%zu.png", keyboard_row);
                if (l2dcat_path_join(keyboard, sizeof(keyboard), keyboard_dir, filename) &&
                    l2dcat_path_is_file(keyboard)) keyboard_path = keyboard;
            }
            int modifier = modifier_index(code);
            size_t occurrence = modifier >= 0 ? modifier_seen[modifier]++ : 0;
            KeyNames names = candidate->mode == L2DCAT_MODE_GAMEPAD
                ? gamepad_names(code) : device_names(code, occurrence,
                    modifier >= 0 ? modifier_total[modifier] : 0);
            if (!l2dcat_legacy_emit_pair(hand, keyboard_path, output_dir, names, error)) return false;
        }
    }
    return true;
}

bool l2dcat_import_legacy_assets(const L2DCatImportCandidate *candidate,
    const char *source_root, const char *target, L2DCatError *error) {
    const char *left = candidate->mode == L2DCAT_MODE_STANDARD ? "hand" : "lefthand";
    if (!child_is_dir(candidate->assets, left)) return true;
    char config_path[L2DCAT_PATH_CAP];
    /* A native model may legitimately use a directory named hand. Only a
       nearby config.json identifies the old Mver package layout. */
    if (!find_config(source_root, config_path)) return true;
    FILE *file = l2dcat_file_open(config_path, "rb");
    yyjson_doc *document = file ? yyjson_read_fp(file,
        YYJSON_READ_JSON5 | YYJSON_READ_ALLOW_INVALID_UNICODE, NULL, NULL) : NULL;
    if (file) fclose(file);
    yyjson_val *root = document ? yyjson_doc_get_root(document) : NULL;
    yyjson_val *mode = yyjson_is_obj(root)
        ? yyjson_obj_get(root, l2dcat_mode_name(candidate->mode)) : NULL;
    if (!yyjson_is_obj(mode)) {
        bool native_layout = strcmp(candidate->assets, candidate->directory) == 0;
        yyjson_doc_free(document);
        if (native_layout) return true;
        l2dcat_error_set(error, L2DCAT_ERROR_FORMAT,
            "Cannot parse legacy input configuration: %s", config_path);
        return false;
    }
    bool ok = false;
    yyjson_val *keyboard = yyjson_obj_get(mode, "keyboard");
    if (candidate->mode == L2DCAT_MODE_STANDARD) {
        yyjson_val *hand = yyjson_obj_get(mode, "hand");
        if (yyjson_is_arr(hand)) ok = process_matrix(candidate, hand, NULL, NULL,
            keyboard, "hand", "left-keys", target, error);
    } else {
        yyjson_val *left_keys = yyjson_obj_get(mode, "lefthand");
        yyjson_val *right_keys = yyjson_obj_get(mode, "righthand");
        if (yyjson_is_arr(left_keys) && yyjson_is_arr(right_keys))
            ok = process_matrix(candidate, left_keys, NULL, right_keys, keyboard,
                "lefthand", "left-keys", target, error) &&
            process_matrix(candidate, right_keys, left_keys, NULL, keyboard,
                "righthand", "right-keys", target, error);
    }
    yyjson_doc_free(document);
    if (!ok && error && !error->message[0]) l2dcat_error_set(error, L2DCAT_ERROR_FORMAT,
        "Cannot convert legacy input configuration: %s", config_path);
    return ok;
}
