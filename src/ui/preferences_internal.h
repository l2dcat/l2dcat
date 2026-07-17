#ifndef BONGO_PREFERENCES_INTERNAL_H
#define BONGO_PREFERENCES_INTERNAL_H

#include "bongo/app.h"
#include "nuklear_config.h"

void bongo_preferences_page_cat(BongoApp *app, struct nk_context *context);
void bongo_preferences_page_general(BongoApp *app, struct nk_context *context);
void bongo_preferences_page_model(BongoApp *app, struct nk_context *context);
void bongo_preferences_page_shortcuts(BongoApp *app, struct nk_context *context);
void bongo_preferences_page_about(BongoApp *app, struct nk_context *context);
void bongo_preferences_import_path(BongoApp *app, SDL_Window *window,
    const char *path);

#endif
