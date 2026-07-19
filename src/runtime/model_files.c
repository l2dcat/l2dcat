#include "runtime.h"
#include "l2dcat/file.h"
#include "l2dcat/overlay.h"
#include "l2dcat/path.h"

#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
