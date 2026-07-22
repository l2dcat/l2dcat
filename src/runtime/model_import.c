#include "model_import.h"
#include "l2dcat/file.h"
#include "l2dcat/path.h"

#include <stdio.h>
#include <string.h>

typedef struct ImportInstall {
    char id[L2DCAT_ID_CAP];
    char temporary[L2DCAT_PATH_CAP];
    char target[L2DCAT_PATH_CAP];
    bool committed;
} ImportInstall;

static bool remove_tree(const char *path, L2DCatError *error);

static SDL_EnumerationResult SDLCALL remove_item(void *userdata,
    const char *dirname, const char *name) {
    L2DCatError *error = userdata;
    char path[L2DCAT_PATH_CAP];
    return l2dcat_path_join(path, sizeof(path), dirname, name) && remove_tree(path, error)
        ? SDL_ENUM_CONTINUE : SDL_ENUM_FAILURE;
}

static bool remove_tree(const char *path, L2DCatError *error) {
    SDL_PathInfo info;
    if (!path[0] || !SDL_GetPathInfo(path, &info)) return true;
    if (info.type == SDL_PATHTYPE_DIRECTORY &&
        !SDL_EnumerateDirectory(path, remove_item, error)) return false;
    if (SDL_RemovePath(path)) return true;
    if (error) l2dcat_error_set(error, L2DCAT_ERROR_IO,
        "Cannot remove %s: %s", path, SDL_GetError());
    return false;
}

static bool custom_root(L2DCatApp *app, char *path, size_t capacity) {
    return app->data_root[0] &&
        l2dcat_path_join(path, capacity, app->data_root, "custom-models") &&
        SDL_CreateDirectory(path);
}

static bool copy_optional(const char *source_dir, const char *source_name,
    const char *target_dir, const char *target_name) {
    char source[L2DCAT_PATH_CAP], target[L2DCAT_PATH_CAP];
    return l2dcat_path_join(source, sizeof(source), source_dir, source_name) &&
        l2dcat_path_join(target, sizeof(target), target_dir, target_name) &&
        SDL_CopyFile(source, target);
}

static bool copy_first(const char *source_dir, const char *const *names, size_t count,
    const char *target_dir, const char *target_name) {
    for (size_t i = 0; i < count; ++i) {
        char source[L2DCAT_PATH_CAP];
        if (!l2dcat_path_join(source, sizeof(source), source_dir, names[i]) ||
            !l2dcat_path_is_file(source)) continue;
        return copy_optional(source_dir, names[i], target_dir, target_name);
    }
    return true;
}

static bool copy_preview_file(const char *source_resources, const char *source_root,
    const char *const *names, size_t count, const char *target_resources,
    const char *target_name) {
    for (size_t i = 0; i < count; ++i) {
        char source[L2DCAT_PATH_CAP];
        if (l2dcat_path_join(source, sizeof(source), source_resources, names[i]) &&
            l2dcat_path_is_file(source))
            return copy_optional(source_resources, names[i], target_resources, target_name);
    }
    return copy_first(source_root, names, count, target_resources, target_name);
}

static bool preview_file_exists(const char *directory, const char *name) {
    char path[L2DCAT_PATH_CAP];
    return l2dcat_path_join(path, sizeof(path), directory, name) &&
        l2dcat_path_is_file(path);
}

static bool copy_preview(const L2DCatImportCandidate *candidate, const char *target,
    L2DCatError *error) {
    if (strcmp(candidate->assets, candidate->directory) == 0) return true;
    char source_resources[L2DCAT_PATH_CAP], target_resources[L2DCAT_PATH_CAP];
    if (!l2dcat_path_join(source_resources, sizeof(source_resources),
        candidate->assets, "resources") ||
        !l2dcat_path_join(target_resources, sizeof(target_resources), target, "resources"))
        return false;
    bool target_exists = l2dcat_path_is_dir(target_resources);
    if (l2dcat_path_is_dir(source_resources) && !target_exists) {
        if (l2dcat_copy_directory(source_resources, target_resources, error) != L2DCAT_OK)
            return false;
        target_exists = true;
    }
    if (!target_exists && !SDL_CreateDirectory(target_resources)) return false;
    const char *covers[] = {"cover.png", "cat.png", "bg.png"};
    const char *backgrounds[] = {"background.png", "bg.png", "mousebg.png",
        "tabletbg.png"};
    bool ok = (preview_file_exists(target_resources, "cover.png") ||
        copy_preview_file(source_resources, candidate->assets, covers, 3,
            target_resources, "cover.png")) &&
        (preview_file_exists(target_resources, "background.png") ||
        copy_preview_file(source_resources, candidate->assets, backgrounds, 4,
            target_resources, "background.png"));
    if (!ok) l2dcat_error_set(error, L2DCAT_ERROR_IO,
        "Cannot copy model preview assets: %s", SDL_GetError());
    return ok;
}

static bool write_mode(const char *target, L2DCatModelMode mode, L2DCatError *error) {
    char path[L2DCAT_PATH_CAP];
    if (!l2dcat_path_join(path, sizeof(path), target, ".l2dcat-mode")) return false;
    FILE *file = l2dcat_file_open(path, "wb");
    if (!file) return false;
    const char *name = mode == L2DCAT_MODE_KEYBOARD ? "keyboard" :
        mode == L2DCAT_MODE_GAMEPAD ? "gamepad" : "standard";
    bool ok = fputs(name, file) >= 0;
    if (fclose(file) != 0) ok = false;
    if (!ok) l2dcat_error_set(error, L2DCAT_ERROR_IO,
        "Cannot write imported model metadata");
    return ok;
}

static void cleanup(ImportInstall *installs, size_t count, bool committed) {
    for (size_t i = 0; i < count; ++i)
        remove_tree(committed && installs[i].committed ? installs[i].target :
            installs[i].temporary, NULL);
}

static bool prepare_install(const L2DCatImportCandidate *candidate,
    ImportInstall *install, const char *source, const char *root, unsigned long long stamp,
    size_t index, size_t count, L2DCatError *error) {
    const char *mode = candidate->mode == L2DCAT_MODE_KEYBOARD ? "keyboard" :
        candidate->mode == L2DCAT_MODE_GAMEPAD ? "gamepad" : "standard";
    if (count == 1) snprintf(install->id, sizeof(install->id), "model-%llx", stamp);
    else snprintf(install->id, sizeof(install->id), "model-%llx-%s-%zu",
        stamp, mode, index + 1);
    char temporary_name[L2DCAT_ID_CAP + 8];
    snprintf(temporary_name, sizeof(temporary_name), ".import-%llx-%zu.tmp",
        stamp, index + 1);
    if (!l2dcat_path_join(install->temporary, sizeof(install->temporary),
        root, temporary_name) ||
        !l2dcat_path_join(install->target, sizeof(install->target), root, install->id) ||
        l2dcat_copy_directory(candidate->directory, install->temporary, error) != L2DCAT_OK ||
        !copy_preview(candidate, install->temporary, error) ||
        !l2dcat_import_legacy_assets(candidate, source, install->temporary, error) ||
        !write_mode(install->temporary, candidate->mode, error)) return false;
    char setting[L2DCAT_PATH_CAP];
    return l2dcat_path_find_suffix(install->temporary, ".model3.json", setting,
        sizeof(setting)) && l2dcat_import_manifest_valid(install->temporary, setting, error);
}

L2DCatResult l2dcat_app_import_model(L2DCatApp *app, const char *source,
    L2DCatError *error) {
    if (!app || !source || !l2dcat_path_is_dir(source)) return L2DCAT_ERROR_ARGUMENT;
    L2DCatImportDiscovery discovery;
    if (!l2dcat_import_discover(source, &discovery, error)) return L2DCAT_ERROR_FORMAT;
    char root[L2DCAT_PATH_CAP];
    if (!custom_root(app, root, sizeof(root))) return L2DCAT_ERROR_IO;
    ImportInstall installs[L2DCAT_IMPORT_CANDIDATE_CAP] = {0};
    unsigned long long stamp = (unsigned long long)SDL_GetTicksNS();
    for (size_t i = 0; i < discovery.count; ++i) {
        if (prepare_install(&discovery.candidates[i], &installs[i], source, root,
            stamp, i, discovery.count, error)) continue;
        cleanup(installs, i + 1, false);
        return error && error->code == L2DCAT_ERROR_FORMAT
            ? L2DCAT_ERROR_FORMAT : L2DCAT_ERROR_IO;
    }
    for (size_t i = 0; i < discovery.count; ++i) {
        if (!SDL_RenamePath(installs[i].temporary, installs[i].target)) {
            l2dcat_error_set(error, L2DCAT_ERROR_IO,
                "Cannot finish model import: %s", SDL_GetError());
            cleanup(installs, discovery.count, true);
            return L2DCAT_ERROR_IO;
        }
        installs[i].committed = true;
    }
    char previous[L2DCAT_PATH_CAP];
    snprintf(previous, sizeof(previous), "%s", app->config.current_model);
    l2dcat_app_rescan_models(app);
    if (l2dcat_app_select_model(app, installs[0].id)) return L2DCAT_OK;
#ifndef L2DCAT_HAS_CUBISM
    const L2DCatModelEntry *entry = l2dcat_models_find(&app->models, installs[0].id);
    if (entry) {
        snprintf(app->config.current_model, sizeof(app->config.current_model),
            "%s", installs[0].id);
        app->config.current_mode = entry->mode;
    }
    return L2DCAT_OK;
#else
    cleanup(installs, discovery.count, true);
    l2dcat_app_rescan_models(app);
    if (previous[0]) l2dcat_app_select_model(app, previous);
    l2dcat_error_set(error, L2DCAT_ERROR_CUBISM,
        "Model import was rolled back because the Live2D model could not be loaded");
    return L2DCAT_ERROR_CUBISM;
#endif
}
