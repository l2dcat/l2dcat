#include "bongo/model.h"
#include "bongo/path.h"

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include "windows_utf8.h"
#include <stdlib.h>
#include <windows.h>
#else
#include <dirent.h>
#endif

void bongo_models_init(BongoModelCatalog *catalog) {
    if (catalog) memset(catalog, 0, sizeof(*catalog));
}

static BongoModelMode infer_mode(const char *directory) {
    const char *name = bongo_path_name(directory);
    if (strcmp(name, "gamepad") == 0) return BONGO_MODE_GAMEPAD;
    if (strcmp(name, "keyboard") == 0) return BONGO_MODE_KEYBOARD;
    if (strcmp(name, "standard") == 0) return BONGO_MODE_STANDARD;
    char path[BONGO_PATH_CAP];
    bongo_path_join(path, sizeof(path), directory, "resources/right-keys/East.png");
    if (bongo_path_is_file(path)) return BONGO_MODE_GAMEPAD;
    bongo_path_join(path, sizeof(path), directory, "resources/right-keys");
    return bongo_path_is_dir(path) ? BONGO_MODE_KEYBOARD : BONGO_MODE_STANDARD;
}

static bool add_model(BongoModelCatalog *catalog, const char *directory, bool preset) {
    if (catalog->count >= BONGO_MODEL_CAP) return false;
    char setting[BONGO_PATH_CAP];
    if (!bongo_path_find_suffix(directory, ".model3.json", setting, sizeof(setting))) return true;
    BongoModelEntry *entry = &catalog->entries[catalog->count++];
    snprintf(entry->id, sizeof(entry->id), "%s", bongo_path_name(directory));
    snprintf(entry->directory, sizeof(entry->directory), "%s", directory);
    snprintf(entry->setting_file, sizeof(entry->setting_file), "%s", setting);
    entry->mode = infer_mode(directory);
    entry->preset = preset;
    return true;
}

#ifdef _WIN32
static BongoResult scan_windows(BongoModelCatalog *catalog, const char *root, bool preset,
    BongoError *error) {
    char pattern[BONGO_PATH_CAP];
    bongo_path_join(pattern, sizeof(pattern), root, "*");
    wchar_t *wide = bongo_windows_wide(pattern);
    WIN32_FIND_DATAW data;
    HANDLE find = wide ? FindFirstFileW(wide, &data) : INVALID_HANDLE_VALUE;
    free(wide);
    if (find == INVALID_HANDLE_VALUE) return BONGO_OK;
    do {
        if (!(data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
        if (wcscmp(data.cFileName, L".") == 0 || wcscmp(data.cFileName, L"..") == 0) continue;
        char filename[BONGO_PATH_CAP];
        if (!bongo_windows_utf8(data.cFileName, filename, sizeof(filename))) continue;
        char directory[BONGO_PATH_CAP];
        if (!bongo_path_join(directory, sizeof(directory), root, filename) ||
            !add_model(catalog, directory, preset)) {
            FindClose(find);
            bongo_error_set(error, BONGO_ERROR_FORMAT, "Too many models or a path is too long");
            return BONGO_ERROR_FORMAT;
        }
    } while (FindNextFileW(find, &data));
    FindClose(find);
    return BONGO_OK;
}
#else

static BongoResult scan_posix(BongoModelCatalog *catalog, const char *root, bool preset,
    BongoError *error) {
    DIR *handle = opendir(root);
    if (!handle) return BONGO_OK;
    struct dirent *item;
    while ((item = readdir(handle))) {
        if (item->d_name[0] == '.') continue;
        char directory[BONGO_PATH_CAP];
        if (!bongo_path_join(directory, sizeof(directory), root, item->d_name)) continue;
        if (!bongo_path_is_dir(directory)) continue;
        if (!add_model(catalog, directory, preset)) {
            closedir(handle);
            bongo_error_set(error, BONGO_ERROR_FORMAT, "Too many models");
            return BONGO_ERROR_FORMAT;
        }
    }
    closedir(handle);
    return BONGO_OK;
}
#endif

BongoResult bongo_models_scan(BongoModelCatalog *catalog, const char *root, bool preset,
    BongoError *error) {
    if (!catalog || !root) return BONGO_ERROR_ARGUMENT;
#ifdef _WIN32
    return scan_windows(catalog, root, preset, error);
#else
    return scan_posix(catalog, root, preset, error);
#endif
}

const BongoModelEntry *bongo_models_find(const BongoModelCatalog *catalog, const char *id) {
    if (!catalog || !id) return NULL;
    for (size_t i = 0; i < catalog->count; ++i) {
        if (strcmp(catalog->entries[i].id, id) == 0) return &catalog->entries[i];
    }
    return NULL;
}
