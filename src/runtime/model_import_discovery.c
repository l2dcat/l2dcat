#include "model_import.h"
#include "l2dcat/file.h"
#include "l2dcat/path.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <yyjson.h>

static bool safe_reference(const char *value) {
    if (!value || !value[0] || value[0] == '/' || value[0] == '\\' ||
        strchr(value, ':')) return false;
    const char *part = value;
    while (*part) {
        while (*part == '/' || *part == '\\') part++;
        if (part[0] == '.' && part[1] == '.' &&
            (!part[2] || part[2] == '/' || part[2] == '\\')) return false;
        part = strpbrk(part, "/\\");
        if (!part) break;
    }
    return true;
}

static bool referenced_file(const char *root, const char *relative) {
    char path[L2DCAT_PATH_CAP];
    return safe_reference(relative) &&
        l2dcat_path_join(path, sizeof(path), root, relative) && l2dcat_path_is_file(path);
}

bool l2dcat_import_manifest_valid(const char *root, const char *setting,
    L2DCatError *error) {
    char path[L2DCAT_PATH_CAP];
    if (!l2dcat_path_join(path, sizeof(path), root, setting)) return false;
    FILE *file = l2dcat_file_open(path, "rb");
    yyjson_doc *document = file ? yyjson_read_fp(file, 0, NULL, NULL) : NULL;
    if (file) fclose(file);
    yyjson_val *manifest = document ? yyjson_doc_get_root(document) : NULL;
    yyjson_val *refs = yyjson_is_obj(manifest)
        ? yyjson_obj_get(manifest, "FileReferences") : NULL;
    const char *moc = yyjson_get_str(yyjson_obj_get(refs, "Moc"));
    yyjson_val *textures = yyjson_obj_get(refs, "Textures");
    bool valid = yyjson_get_int(yyjson_obj_get(manifest, "Version")) == 3 &&
        yyjson_is_obj(refs) && referenced_file(root, moc) && yyjson_is_arr(textures) &&
        yyjson_arr_size(textures) > 0;
    size_t index, maximum; yyjson_val *texture;
    yyjson_arr_foreach(textures, index, maximum, texture)
        valid = valid && referenced_file(root, yyjson_get_str(texture));
    yyjson_doc_free(document);
    if (!valid && error) l2dcat_error_set(error, L2DCAT_ERROR_FORMAT,
        "Model manifest or referenced assets are invalid: %s", path);
    return valid;
}

static bool path_parent(const char *path, char *parent, size_t capacity) {
    size_t length = path ? strlen(path) : 0;
    while (length && (path[length - 1] == '/' || path[length - 1] == '\\')) length--;
    while (length && path[length - 1] != '/' && path[length - 1] != '\\') length--;
    while (length > 1 && (path[length - 1] == '/' || path[length - 1] == '\\')) length--;
    if (!length || length >= capacity) return false;
    memcpy(parent, path, length);
    parent[length] = '\0';
    return true;
}

static L2DCatModelMode import_mode(const char *path) {
    const char *cursor = path;
    L2DCatModelMode mode = L2DCAT_MODE_STANDARD;
    while (cursor && *cursor) {
        while (*cursor == '/' || *cursor == '\\') cursor++;
        const char *end = strpbrk(cursor, "/\\");
        size_t length = end ? (size_t)(end - cursor) : strlen(cursor);
        if (length == 8 && SDL_strncasecmp(cursor, "keyboard", length) == 0)
            mode = L2DCAT_MODE_KEYBOARD;
        else if (length == 7 && SDL_strncasecmp(cursor, "gamepad", length) == 0)
            mode = L2DCAT_MODE_GAMEPAD;
        else if (length == 8 && SDL_strncasecmp(cursor, "standard", length) == 0)
            mode = L2DCAT_MODE_STANDARD;
        if (!end) break;
        cursor = end + 1;
    }
    return mode;
}

static bool has_preview_assets(const char *directory) {
    const char *names[] = {"resources", "cover.png", "cat.png", "bg.png",
        "mousebg.png", "tabletbg.png"};
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {
        char path[L2DCAT_PATH_CAP];
        if (!l2dcat_path_join(path, sizeof(path), directory, names[i])) continue;
        if (l2dcat_path_is_file(path) || l2dcat_path_is_dir(path)) return true;
    }
    return false;
}

static bool add_candidate(L2DCatImportDiscovery *discovery, const char *directory,
    const char *setting) {
    if (discovery->count >= L2DCAT_IMPORT_CANDIDATE_CAP) return false;
    for (size_t i = 0; i < discovery->count; ++i)
        if (strcmp(discovery->candidates[i].directory, directory) == 0 &&
            strcmp(discovery->candidates[i].setting, setting) == 0) return true;
    L2DCatImportCandidate *candidate = &discovery->candidates[discovery->count++];
    snprintf(candidate->directory, sizeof(candidate->directory), "%s", directory);
    snprintf(candidate->setting, sizeof(candidate->setting), "%s", setting);
    snprintf(candidate->assets, sizeof(candidate->assets), "%s", directory);
    char parent[L2DCAT_PATH_CAP];
    if (path_parent(directory, parent, sizeof(parent)) && has_preview_assets(parent))
        snprintf(candidate->assets, sizeof(candidate->assets), "%s", parent);
    candidate->mode = import_mode(candidate->assets);
    return true;
}

static bool suffix(const char *name, const char *ending) {
    size_t a = name ? strlen(name) : 0, b = ending ? strlen(ending) : 0;
    return a >= b && strcmp(name + a - b, ending) == 0;
}

static SDL_EnumerationResult SDLCALL discover_item(void *userdata,
    const char *dirname, const char *name) {
    L2DCatImportDiscovery *discovery = userdata;
    char path[L2DCAT_PATH_CAP];
    if (!l2dcat_path_join(path, sizeof(path), dirname, name)) return SDL_ENUM_FAILURE;
    SDL_PathInfo info;
    if (!SDL_GetPathInfo(path, &info)) return SDL_ENUM_CONTINUE;
    if (info.type == SDL_PATHTYPE_FILE && suffix(name, ".model3.json")) {
        if (l2dcat_import_manifest_valid(dirname, name, NULL) &&
            !add_candidate(discovery, dirname, name)) return SDL_ENUM_FAILURE;
        return SDL_ENUM_CONTINUE;
    }
    if (info.type != SDL_PATHTYPE_DIRECTORY || discovery->depth >= 8 || name[0] == '.')
        return SDL_ENUM_CONTINUE;
    discovery->depth++;
    bool ok = SDL_EnumerateDirectory(path, discover_item, discovery);
    discovery->depth--;
    return ok ? SDL_ENUM_CONTINUE : SDL_ENUM_FAILURE;
}

static int rank(const L2DCatImportCandidate *candidate) {
    return candidate->mode == L2DCAT_MODE_STANDARD ? 0 :
        candidate->mode == L2DCAT_MODE_KEYBOARD ? 1 : 2;
}

static int compare_candidates(const void *left, const void *right) {
    const L2DCatImportCandidate *a = left, *b = right;
    int difference = rank(a) - rank(b);
    return difference ? difference : strcmp(a->directory, b->directory);
}

bool l2dcat_import_discover(const char *source, L2DCatImportDiscovery *discovery,
    L2DCatError *error) {
    memset(discovery, 0, sizeof(*discovery));
    char direct[L2DCAT_PATH_CAP];
    if (l2dcat_path_find_suffix(source, ".model3.json", direct, sizeof(direct)) &&
        l2dcat_import_manifest_valid(source, direct, NULL))
        return add_candidate(discovery, source, direct);
    if (!SDL_EnumerateDirectory(source, discover_item, discovery)) {
        l2dcat_error_set(error, L2DCAT_ERROR_FORMAT,
            "Cannot scan model directory or it contains too many models");
        return false;
    }
    if (!discovery->count) {
        l2dcat_error_set(error, L2DCAT_ERROR_FORMAT,
            "Selected directory contains no valid Live2D model3 JSON");
        return false;
    }
    qsort(discovery->candidates, discovery->count,
        sizeof(discovery->candidates[0]), compare_candidates);
    return true;
}
