#ifndef L2DCAT_PREFERENCES_INTERNAL_H
#define L2DCAT_PREFERENCES_INTERNAL_H

#include "l2dcat/app.h"
#include "nuklear_config.h"

void l2dcat_preferences_page_cat(L2DCatApp *app, struct nk_context *context);
void l2dcat_preferences_page_general(L2DCatApp *app, struct nk_context *context);
void l2dcat_preferences_page_model(L2DCatApp *app, struct nk_context *context);
void l2dcat_preferences_page_shortcuts(L2DCatApp *app, struct nk_context *context);
void l2dcat_preferences_page_about(L2DCatApp *app, struct nk_context *context);
void l2dcat_preferences_import_path(L2DCatApp *app, SDL_Window *window,
    const char *path);
void l2dcat_preferences_model_cache_clear(L2DCatApp *app);

#endif
