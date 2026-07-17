#include "runtime.h"
#include "bongo/file.h"
#include "bongo/overlay.h"
#include "bongo/path.h"

#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>
#include <yyjson.h>

typedef struct TreeContext {
    const char *source;
    const char *target;
    BongoError *error;
} TreeContext;

bool bongo_app_select_model(BongoApp *app, const char *id) {
    if (!app || !app->live2d || !id) return false;
    const BongoModelEntry *entry = bongo_models_find(&app->models, id);
    if (!entry) return false;
    BongoError error = {0};
    BongoBehaviorCatalog behaviors = {0};
    if (bongo_behaviors_load(&behaviors, entry, &error) != BONGO_OK)
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "%s", error.message);
    if (bongo_live2d_load(app->live2d, entry->directory,
        entry->setting_file, &error) != BONGO_OK) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", error.message);
        return false;
    }
    app->behaviors = behaviors;
    bongo_overlay_load(app->overlay, entry->directory, &error);
    snprintf(app->config.current_model, sizeof(app->config.current_model), "%s", entry->id);
    app->config.current_mode = entry->mode;
    bongo_gamepads_set_enabled(app, entry->mode == BONGO_MODE_GAMEPAD);
    int pixel_width = app->config.window.width, pixel_height = app->config.window.height;
    if (app->window) SDL_GetWindowSizeInPixels(app->window, &pixel_width, &pixel_height);
    bongo_live2d_resize(app->live2d, pixel_width, pixel_height);
    app->dirty = true;
    return true;
}

static bool copy_tree(const char *source, const char *target, BongoError *error);
static bool remove_tree(const char *path, BongoError *error);

static SDL_EnumerationResult SDLCALL copy_item(void *userdata,
    const char *dirname, const char *name) {
    (void)dirname;
    TreeContext *context = userdata;
    char source[BONGO_PATH_CAP], target[BONGO_PATH_CAP];
    if (!bongo_path_join(source, sizeof(source), context->source, name) ||
        !bongo_path_join(target, sizeof(target), context->target, name)) {
        bongo_error_set(context->error, BONGO_ERROR_IO, "Model path is too long");
        return SDL_ENUM_FAILURE;
    }
    SDL_PathInfo info;
    if (!SDL_GetPathInfo(source, &info)) {
        bongo_error_set(context->error, BONGO_ERROR_IO, "%s", SDL_GetError());
        return SDL_ENUM_FAILURE;
    }
    bool ok = info.type == SDL_PATHTYPE_DIRECTORY
        ? copy_tree(source, target, context->error)
        : info.type == SDL_PATHTYPE_FILE && SDL_CopyFile(source, target);
    if (!ok && !context->error->message[0])
        bongo_error_set(context->error, BONGO_ERROR_IO, "Cannot copy %s: %s",
            source, SDL_GetError());
    return ok ? SDL_ENUM_CONTINUE : SDL_ENUM_FAILURE;
}

static bool copy_tree(const char *source, const char *target, BongoError *error) {
    if (!SDL_CreateDirectory(target)) {
        bongo_error_set(error, BONGO_ERROR_IO, "Cannot create %s: %s", target, SDL_GetError());
        return false;
    }
    TreeContext context = {source, target, error};
    return SDL_EnumerateDirectory(source, copy_item, &context);
}

BongoResult bongo_copy_directory(const char *source, const char *target,
    BongoError *error) {
    if (!source || !target || !bongo_path_is_dir(source)) return BONGO_ERROR_ARGUMENT;
    return copy_tree(source, target, error) ? BONGO_OK : BONGO_ERROR_IO;
}

static SDL_EnumerationResult SDLCALL remove_item(void *userdata,
    const char *dirname, const char *name) {
    BongoError *error = userdata;
    char path[BONGO_PATH_CAP];
    if (!bongo_path_join(path, sizeof(path), dirname, name) || !remove_tree(path, error))
        return SDL_ENUM_FAILURE;
    return SDL_ENUM_CONTINUE;
}

static bool remove_tree(const char *path, BongoError *error) {
    SDL_PathInfo info;
    if (!SDL_GetPathInfo(path, &info)) return true;
    if (info.type == SDL_PATHTYPE_DIRECTORY &&
        !SDL_EnumerateDirectory(path, remove_item, error)) return false;
    if (!SDL_RemovePath(path)) {
        bongo_error_set(error, BONGO_ERROR_IO, "Cannot remove %s: %s", path, SDL_GetError());
        return false;
    }
    return true;
}

static bool custom_root(BongoApp *app, char *path, size_t capacity) {
    return app->data_root[0] &&
        bongo_path_join(path, capacity, app->data_root, "custom-models") &&
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
    char path[BONGO_PATH_CAP];
    return safe_reference(relative) &&
        bongo_path_join(path, sizeof(path), root, relative) && bongo_path_is_file(path);
}

static bool valid_manifest(const char *root, const char *setting, BongoError *error) {
    char path[BONGO_PATH_CAP];
    if (!bongo_path_join(path, sizeof(path), root, setting)) return false;
    FILE *file = bongo_file_open(path, "rb");
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
    valid = valid && referenced_file(root, "resources/background.png") &&
        referenced_file(root, "resources/cover.png");
    yyjson_doc_free(document);
    if (!valid) bongo_error_set(error, BONGO_ERROR_FORMAT,
        "Model manifest or referenced assets are invalid");
    return valid;
}

void bongo_app_rescan_models(BongoApp *app) {
    if (!app) return;
    BongoError error = {0};
    char root[BONGO_PATH_CAP];
    bongo_models_init(&app->models);
    bongo_path_join(root, sizeof(root), app->asset_root, "models");
    bongo_models_scan(&app->models, root, true, &error);
    if (custom_root(app, root, sizeof(root)))
        bongo_models_scan(&app->models, root, false, &error);
}

BongoResult bongo_app_import_model(BongoApp *app, const char *source,
    BongoError *error) {
    if (!app || !source || !bongo_path_is_dir(source)) return BONGO_ERROR_ARGUMENT;
    char setting[BONGO_PATH_CAP];
    if (!bongo_path_find_suffix(source, ".model3.json", setting, sizeof(setting))) {
        bongo_error_set(error, BONGO_ERROR_FORMAT, "Selected directory has no model3 JSON");
        return BONGO_ERROR_FORMAT;
    }
    if (!valid_manifest(source, setting, error)) return BONGO_ERROR_FORMAT;
    char root[BONGO_PATH_CAP], id[BONGO_ID_CAP], temporary[BONGO_PATH_CAP], target[BONGO_PATH_CAP];
    if (!custom_root(app, root, sizeof(root))) return BONGO_ERROR_IO;
    snprintf(id, sizeof(id), "model-%llx", (unsigned long long)SDL_GetTicksNS());
    char temporary_name[BONGO_ID_CAP + 8];
    snprintf(temporary_name, sizeof(temporary_name), ".%s.tmp", id);
    if (!bongo_path_join(temporary, sizeof(temporary), root, temporary_name) ||
        !bongo_path_join(target, sizeof(target), root, id) ||
        !copy_tree(source, temporary, error)) {
        remove_tree(temporary, error);
        return BONGO_ERROR_IO;
    }
    if (!SDL_RenamePath(temporary, target)) {
        bongo_error_set(error, BONGO_ERROR_IO, "Cannot finish model import: %s", SDL_GetError());
        remove_tree(temporary, error);
        return BONGO_ERROR_IO;
    }
    bongo_app_rescan_models(app);
    if (bongo_app_select_model(app, id)) return BONGO_OK;
#ifndef BONGO_HAS_CUBISM
    const BongoModelEntry *entry = bongo_models_find(&app->models, id);
    if (entry) {
        snprintf(app->config.current_model, sizeof(app->config.current_model), "%s", id);
        app->config.current_mode = entry->mode;
    }
    return BONGO_OK;
#else
    bongo_error_set(error, BONGO_ERROR_CUBISM, "Model was copied but could not be loaded");
    return BONGO_ERROR_CUBISM;
#endif
}

BongoResult bongo_app_remove_model(BongoApp *app, const char *id,
    BongoError *error) {
    if (!app || !id) {
        bongo_error_set(error, BONGO_ERROR_ARGUMENT, "Missing model id");
        return BONGO_ERROR_ARGUMENT;
    }
    const BongoModelEntry *entry = bongo_models_find(&app->models, id);
    if (!entry) {
        bongo_error_set(error, BONGO_ERROR_ARGUMENT, "Model is not installed: %s", id);
        return BONGO_ERROR_ARGUMENT;
    }
    if (entry->preset) {
        bongo_error_set(error, BONGO_ERROR_ARGUMENT, "Built-in models cannot be removed: %s", id);
        return BONGO_ERROR_ARGUMENT;
    }
    bool selected = strcmp(id, app->config.current_model) == 0;
    char directory[BONGO_PATH_CAP];
    snprintf(directory, sizeof(directory), "%s", entry->directory);
    if (!remove_tree(directory, error)) return BONGO_ERROR_IO;
    bongo_app_rescan_models(app);
    if (selected && app->models.count) bongo_app_select_model(app, app->models.entries[0].id);
    return BONGO_OK;
}
