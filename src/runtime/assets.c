#include "runtime.h"
#include "l2dcat/path.h"
#include "l2dcat/platform.h"

#include <stdio.h>

static bool asset_directory(char *output, size_t capacity, const char *root) {
    return l2dcat_path_join(output, capacity, root, "assets") &&
        l2dcat_path_is_dir(output);
}

void l2dcat_app_locate_assets(L2DCatApp *app) {
    const char *base = SDL_GetBasePath();
    if (!asset_directory(app->asset_root, sizeof(app->asset_root), base ? base : "")) {
        char cache[L2DCAT_PATH_CAP], name[64];
        snprintf(name, sizeof(name), "embedded-assets-%s", L2DCAT_VERSION);
        if (l2dcat_path_join(cache, sizeof(cache), app->data_root, name)) {
            L2DCatError error = {0};
            if (l2dcat_platform_embedded_assets(cache, &error) == L2DCAT_OK) {
                asset_directory(app->asset_root, sizeof(app->asset_root), cache);
            } else if (error.message[0]) {
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "%s", error.message);
            }
        }
    }
    if (!l2dcat_path_is_dir(app->asset_root))
        snprintf(app->asset_root, sizeof(app->asset_root), "%s", L2DCAT_DEV_ASSET_ROOT);
    if (!l2dcat_path_join(app->locale_root, sizeof(app->locale_root),
            app->asset_root, "locales") || !l2dcat_path_is_dir(app->locale_root))
        snprintf(app->locale_root, sizeof(app->locale_root), "%s", L2DCAT_DEV_LOCALE_ROOT);
}
