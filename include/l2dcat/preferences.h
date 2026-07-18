#ifndef L2DCAT_PREFERENCES_H
#define L2DCAT_PREFERENCES_H

#include "l2dcat/common.h"

typedef union SDL_Event SDL_Event;
typedef struct L2DCatApp L2DCatApp;
typedef struct L2DCatPreferences L2DCatPreferences;

L2DCatPreferences *l2dcat_preferences_create(L2DCatApp *app);
void l2dcat_preferences_destroy(L2DCatPreferences *preferences);
void l2dcat_preferences_show(L2DCatPreferences *preferences);
void l2dcat_preferences_close(L2DCatPreferences *preferences);
bool l2dcat_preferences_visible(const L2DCatPreferences *preferences);
void l2dcat_preferences_input_begin(L2DCatPreferences *preferences);
void l2dcat_preferences_input_end(L2DCatPreferences *preferences);
bool l2dcat_preferences_event(L2DCatPreferences *preferences, const SDL_Event *event);
void l2dcat_preferences_render(L2DCatPreferences *preferences);
void l2dcat_preferences_request_model_import(L2DCatPreferences *preferences);

#endif
