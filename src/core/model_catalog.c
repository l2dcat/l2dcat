#include "l2dcat/model.h"
#include "l2dcat/path.h"

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include "windows_utf8.h"
#include <stdlib.h>
#include <windows.h>
#else
#include <dirent.h>
#endif

void l2dcat_models_init(L2DCatModelCatalog *catalog) {
    if (catalog) memset(catalog, 0, sizeof(*catalog));
}

static L2DCatModelMode infer_mode(const char *directory) {
    const char *name = l2dcat_path_name(directory);
    if (strcmp(name, "gamepad") == 0) return L2DCAT_MODE_GAMEPAD;
    if (strcmp(name, "keyboard") == 0) return L2DCAT_MODE_KEYBOARD;
    if (strcmp(name, "standard") == 0) return L2DCAT_MODE_STANDARD;
    char path[L2DCAT_PATH_CAP];
    l2dcat_path_join(path, sizeof(path), directory, "resources/right-keys/East.png");
    if (l2dcat_path_is_file(path)) return L2DCAT_MODE_GAMEPAD;
    l2dcat_path_join(path, sizeof(path), directory, "resources/right-keys");
    return l2dcat_path_is_dir(path) ? L2DCAT_MODE_KEYBOARD : L2DCAT_MODE_STANDARD;
}

static bool add_model(L2DCatModelCatalog *catalog, const char *directory, bool preset) {
    if (catalog->count >= L2DCAT_MODEL_CAP) return false;
    char setting[L2DCAT_PATH_CAP];
    if (!l2dcat_path_find_suffix(directory, ".model3.json", setting, sizeof(setting))) return true;
    L2DCatModelEntry *entry = &catalog->entries[catalog->count++];
    snprintf(entry->id, sizeof(entry->id), "%s", l2dcat_path_name(directory));
    snprintf(entry->directory, sizeof(entry->directory), "%s", directory);
    snprintf(entry->setting_file, sizeof(entry->setting_file), "%s", setting);
    entry->mode = infer_mode(directory);
    entry->preset = preset;
    return true;
}

#ifdef _WIN32
static L2DCatResult scan_windows(L2DCatModelCatalog *catalog, const char *root, bool preset,
    L2DCatError *error) {
    char pattern[L2DCAT_PATH_CAP];
    l2dcat_path_join(pattern, sizeof(pattern), root, "*");
    wchar_t *wide = l2dcat_windows_wide(pattern);
    WIN32_FIND_DATAW data;
    HANDLE find = wide ? FindFirstFileW(wide, &data) : INVALID_HANDLE_VALUE;
    free(wide);
    if (find == INVALID_HANDLE_VALUE) return L2DCAT_OK;
    do {
        if (!(data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
        if (wcscmp(data.cFileName, L".") == 0 || wcscmp(data.cFileName, L"..") == 0) continue;
        char filename[L2DCAT_PATH_CAP];
        if (!l2dcat_windows_utf8(data.cFileName, filename, sizeof(filename))) continue;
        char directory[L2DCAT_PATH_CAP];
        if (!l2dcat_path_join(directory, sizeof(directory), root, filename) ||
            !add_model(catalog, directory, preset)) {
            FindClose(find);
            l2dcat_error_set(error, L2DCAT_ERROR_FORMAT, "Too many models or a path is too long");
            return L2DCAT_ERROR_FORMAT;
        }
    } while (FindNextFileW(find, &data));
    FindClose(find);
    return L2DCAT_OK;
}
#else

static L2DCatResult scan_posix(L2DCatModelCatalog *catalog, const char *root, bool preset,
    L2DCatError *error) {
    DIR *handle = opendir(root);
    if (!handle) return L2DCAT_OK;
    struct dirent *item;
    while ((item = readdir(handle))) {
        if (item->d_name[0] == '.') continue;
        char directory[L2DCAT_PATH_CAP];
        if (!l2dcat_path_join(directory, sizeof(directory), root, item->d_name)) continue;
        if (!l2dcat_path_is_dir(directory)) continue;
        if (!add_model(catalog, directory, preset)) {
            closedir(handle);
            l2dcat_error_set(error, L2DCAT_ERROR_FORMAT, "Too many models");
            return L2DCAT_ERROR_FORMAT;
        }
    }
    closedir(handle);
    return L2DCAT_OK;
}
#endif

L2DCatResult l2dcat_models_scan(L2DCatModelCatalog *catalog, const char *root, bool preset,
    L2DCatError *error) {
    if (!catalog || !root) return L2DCAT_ERROR_ARGUMENT;
#ifdef _WIN32
    return scan_windows(catalog, root, preset, error);
#else
    return scan_posix(catalog, root, preset, error);
#endif
}

const L2DCatModelEntry *l2dcat_models_find(const L2DCatModelCatalog *catalog, const char *id) {
    if (!catalog || !id) return NULL;
    for (size_t i = 0; i < catalog->count; ++i) {
        if (strcmp(catalog->entries[i].id, id) == 0) return &catalog->entries[i];
    }
    return NULL;
}
