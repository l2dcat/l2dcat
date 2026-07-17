#ifndef BONGO_PREFERENCES_H
#define BONGO_PREFERENCES_H

#include "bongo/common.h"

typedef union SDL_Event SDL_Event;
typedef struct BongoApp BongoApp;
typedef struct BongoPreferences BongoPreferences;

BongoPreferences *bongo_preferences_create(BongoApp *app);
void bongo_preferences_destroy(BongoPreferences *preferences);
void bongo_preferences_show(BongoPreferences *preferences);
void bongo_preferences_close(BongoPreferences *preferences);
bool bongo_preferences_visible(const BongoPreferences *preferences);
void bongo_preferences_input_begin(BongoPreferences *preferences);
void bongo_preferences_input_end(BongoPreferences *preferences);
bool bongo_preferences_event(BongoPreferences *preferences, const SDL_Event *event);
void bongo_preferences_render(BongoPreferences *preferences);
void bongo_preferences_request_model_import(BongoPreferences *preferences);

#endif
