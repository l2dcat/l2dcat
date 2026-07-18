#include "runtime.h"
#include "l2dcat/file.h"
#include "l2dcat/overlay.h"
#include "l2dcat/path.h"

#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>
#include <yyjson.h>

typedef struct TreeContext {
    const char *source;
    const char *target;
    L2DCatError *error;
} TreeContext;

bool l2dcat_app_select_model(L2DCatApp *app, const char *id) {
    if (!app || !app->live2d || !id) return false;
    const L2DCatModelEntry *entry = l2dcat_models_find(&app->models, id);
    if (!entry) return false;
    L2DCatError error = {0};
    L2DCatBehaviorCatalog *behaviors = calloc(1, sizeof(*behaviors));
    if (!behaviors) return false;
    if (l2dcat_behaviors_load(behaviors, entry, &error) != L2DCAT_OK)
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "%s", error.message);
    if (l2dcat_live2d_load(app->live2d, entry->directory,
        entry->setting_file, &error) != L2DCAT_OK) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", error.message);
        free(behaviors);
        return false;
    }
    app->behaviors = *behaviors;
    free(behaviors);
    l2dcat_overlay_load(app->overlay, entry->directory, &error);
    snprintf(app->config.current_model, sizeof(app->config.current_model), "%s", entry->id);
    app->config.current_mode = entry->mode;
    l2dcat_gamepads_set_enabled(app, entry->mode == L2DCAT_MODE_GAMEPAD);
    int pixel_width = app->config.window.width, pixel_height = app->config.window.height;
    if (app->window) SDL_GetWindowSizeInPixels(app->window, &pixel_width, &pixel_height);
    l2dcat_live2d_resize(app->live2d, pixel_width, pixel_height);
    app->dirty = true;
    return true;
}

static bool copy_tree(const char *source, const char *target, L2DCatError *error);
static bool remove_tree(const char *path, L2DCatError *error);

static SDL_EnumerationResult SDLCALL copy_item(void *userdata,
    const char *dirname, const char *name) {
    (void)dirname;
    TreeContext *context = userdata;
    char source[L2DCAT_PATH_CAP], target[L2DCAT_PATH_CAP];
    if (!l2dcat_path_join(source, sizeof(source), context->source, name) ||
        !l2dcat_path_join(target, sizeof(target), context->target, name)) {
        l2dcat_error_set(context->error, L2DCAT_ERROR_IO, "Model path is too long");
        return SDL_ENUM_FAILURE;
    }
    SDL_PathInfo info;
    if (!SDL_GetPathInfo(source, &info)) {
        l2dcat_error_set(context->error, L2DCAT_ERROR_IO, "%s", SDL_GetError());
        return SDL_ENUM_FAILURE;
    }
    bool ok = info.type == SDL_PATHTYPE_DIRECTORY
        ? copy_tree(source, target, context->error)
        : info.type == SDL_PATHTYPE_FILE && SDL_CopyFile(source, target);
    if (!ok && !context->error->message[0])
        l2dcat_error_set(context->error, L2DCAT_ERROR_IO, "Cannot copy %s: %s",
            source, SDL_GetError());
    return ok ? SDL_ENUM_CONTINUE : SDL_ENUM_FAILURE;
}

static bool copy_tree(const char *source, const char *target, L2DCatError *error) {
    if (!SDL_CreateDirectory(target)) {
        l2dcat_error_set(error, L2DCAT_ERROR_IO, "Cannot create %s: %s", target, SDL_GetError());
        return false;
    }
    TreeContext context = {source, target, error};
    return SDL_EnumerateDirectory(source, copy_item, &context);
}

L2DCatResult l2dcat_copy_directory(const char *source, const char *target,
    L2DCatError *error) {
    if (!source || !target || !l2dcat_path_is_dir(source)) return L2DCAT_ERROR_ARGUMENT;
    return copy_tree(source, target, error) ? L2DCAT_OK : L2DCAT_ERROR_IO;
}

static SDL_EnumerationResult SDLCALL remove_item(void *userdata,
    const char *dirname, const char *name) {
    L2DCatError *error = userdata;
    char path[L2DCAT_PATH_CAP];
    if (!l2dcat_path_join(path, sizeof(path), dirname, name) || !remove_tree(path, error))
        return SDL_ENUM_FAILURE;
    return SDL_ENUM_CONTINUE;
}

static bool remove_tree(const char *path, L2DCatError *error) {
    SDL_PathInfo info;
    if (!SDL_GetPathInfo(path, &info)) return true;
    if (info.type == SDL_PATHTYPE_DIRECTORY &&
        !SDL_EnumerateDirectory(path, remove_item, error)) return false;
    if (!SDL_RemovePath(path)) {
        l2dcat_error_set(error, L2DCAT_ERROR_IO, "Cannot remove %s: %s", path, SDL_GetError());
        return false;
    }
    return true;
}

static bool custom_root(L2DCatApp *app, char *path, size_t capacity) {
    return app->data_root[0] &&
        l2dcat_path_join(path, capacity, app->data_root, "custom-models") &&
        SDL_CreateDirectory(path);
}

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

static bool valid_manifest(const char *root, const char *setting, L2DCatError *error) {
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
        yyjson_arr_size(textures) > 0 &&
        referenced_file(root, "resources/cover.png") &&
        referenced_file(root, "resources/background.png");
    size_t index, maximum; yyjson_val *texture;
    yyjson_arr_foreach(textures, index, maximum, texture)
        valid = valid && referenced_file(root, yyjson_get_str(texture));
    yyjson_doc_free(document);
    if (!valid) l2dcat_error_set(error, L2DCAT_ERROR_FORMAT,
        "Model manifest or referenced assets are invalid");
    return valid;
}

void l2dcat_app_rescan_models(L2DCatApp *app) {
    if (!app) return;
    L2DCatError error = {0};
    char root[L2DCAT_PATH_CAP];
    l2dcat_models_init(&app->models);
    l2dcat_path_join(root, sizeof(root), app->asset_root, "models");
    l2dcat_models_scan(&app->models, root, true, &error);
    if (custom_root(app, root, sizeof(root)))
        l2dcat_models_scan(&app->models, root, false, &error);
}

L2DCatResult l2dcat_app_import_model(L2DCatApp *app, const char *source,
    L2DCatError *error) {
    if (!app || !source || !l2dcat_path_is_dir(source)) return L2DCAT_ERROR_ARGUMENT;
    char setting[L2DCAT_PATH_CAP];
    if (!l2dcat_path_find_suffix(source, ".model3.json", setting, sizeof(setting))) {
        l2dcat_error_set(error, L2DCAT_ERROR_FORMAT, "Selected directory has no model3 JSON");
        return L2DCAT_ERROR_FORMAT;
    }
    if (!valid_manifest(source, setting, error)) return L2DCAT_ERROR_FORMAT;
    char root[L2DCAT_PATH_CAP], id[L2DCAT_ID_CAP], temporary[L2DCAT_PATH_CAP], target[L2DCAT_PATH_CAP];
    if (!custom_root(app, root, sizeof(root))) return L2DCAT_ERROR_IO;
    snprintf(id, sizeof(id), "model-%llx", (unsigned long long)SDL_GetTicksNS());
    char temporary_name[L2DCAT_ID_CAP + 8];
    snprintf(temporary_name, sizeof(temporary_name), ".%s.tmp", id);
    if (!l2dcat_path_join(temporary, sizeof(temporary), root, temporary_name) ||
        !l2dcat_path_join(target, sizeof(target), root, id) ||
        !copy_tree(source, temporary, error)) {
        remove_tree(temporary, error);
        return L2DCAT_ERROR_IO;
    }
    if (!SDL_RenamePath(temporary, target)) {
        l2dcat_error_set(error, L2DCAT_ERROR_IO, "Cannot finish model import: %s", SDL_GetError());
        remove_tree(temporary, error);
        return L2DCAT_ERROR_IO;
    }
    l2dcat_app_rescan_models(app);
    if (l2dcat_app_select_model(app, id)) return L2DCAT_OK;
#ifndef L2DCAT_HAS_CUBISM
    const L2DCatModelEntry *entry = l2dcat_models_find(&app->models, id);
    if (entry) {
        snprintf(app->config.current_model, sizeof(app->config.current_model), "%s", id);
        app->config.current_mode = entry->mode;
    }
    return L2DCAT_OK;
#else
    l2dcat_error_set(error, L2DCAT_ERROR_CUBISM, "Model was copied but could not be loaded");
    return L2DCAT_ERROR_CUBISM;
#endif
}

L2DCatResult l2dcat_app_remove_model(L2DCatApp *app, const char *id,
    L2DCatError *error) {
    if (!app || !id) {
        l2dcat_error_set(error, L2DCAT_ERROR_ARGUMENT, "Missing model id");
        return L2DCAT_ERROR_ARGUMENT;
    }
    const L2DCatModelEntry *entry = l2dcat_models_find(&app->models, id);
    if (!entry) {
        l2dcat_error_set(error, L2DCAT_ERROR_ARGUMENT, "Model is not installed: %s", id);
        return L2DCAT_ERROR_ARGUMENT;
    }
    if (entry->preset) {
        l2dcat_error_set(error, L2DCAT_ERROR_ARGUMENT, "Built-in models cannot be removed: %s", id);
        return L2DCAT_ERROR_ARGUMENT;
    }
    bool selected = strcmp(id, app->config.current_model) == 0;
    char directory[L2DCAT_PATH_CAP];
    snprintf(directory, sizeof(directory), "%s", entry->directory);
    if (!remove_tree(directory, error)) return L2DCAT_ERROR_IO;
    l2dcat_app_rescan_models(app);
    if (selected && app->models.count) l2dcat_app_select_model(app, app->models.entries[0].id);
    return L2DCAT_OK;
}
