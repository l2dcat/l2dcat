#include "runtime.h"
#include "bongo_cat_neo/path.h"
#include "bongo_cat_neo/platform.h"

#include <stdio.h>

static bool asset_directory(char *output, size_t capacity, const char *root) {
    return bongo_cat_neo_path_join(output, capacity, root, "assets") &&
        bongo_cat_neo_path_is_dir(output);
}

void bongo_cat_neo_app_locate_assets(BongoCatNeoApp *app) {
    const char *base = SDL_GetBasePath();
    if (!asset_directory(app->asset_root, sizeof(app->asset_root), base ? base : "")) {
        char cache[BONGO_CAT_NEO_PATH_CAP], name[64];
        snprintf(name, sizeof(name), "embedded-assets-%s", BONGO_CAT_NEO_VERSION);
        if (bongo_cat_neo_path_join(cache, sizeof(cache), app->data_root, name)) {
            BongoCatNeoError error = {0};
            if (bongo_cat_neo_platform_embedded_assets(cache, &error) == BONGO_CAT_NEO_OK) {
                asset_directory(app->asset_root, sizeof(app->asset_root), cache);
            } else if (error.message[0]) {
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "%s", error.message);
            }
        }
    }
    if (!bongo_cat_neo_path_is_dir(app->asset_root))
        snprintf(app->asset_root, sizeof(app->asset_root), "%s", BONGO_CAT_NEO_DEV_ASSET_ROOT);
    if (!bongo_cat_neo_path_join(app->locale_root, sizeof(app->locale_root),
            app->asset_root, "locales") || !bongo_cat_neo_path_is_dir(app->locale_root))
        snprintf(app->locale_root, sizeof(app->locale_root), "%s", BONGO_CAT_NEO_DEV_LOCALE_ROOT);
}
