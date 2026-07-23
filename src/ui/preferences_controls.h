#ifndef L2DCAT_PREFERENCES_CONTROLS_H
#define L2DCAT_PREFERENCES_CONTROLS_H

#include <stdbool.h>
#include "nuklear_config.h"

bool l2dcat_pref_control_float(struct nk_context *context, const char *id,
    float minimum, float *value, float maximum, float step);
bool l2dcat_pref_control_int(struct nk_context *context, const char *id,
    int minimum, int *value, int maximum, int step);
bool l2dcat_pref_control_slider(struct nk_context *context, const char *id,
    float minimum, float *value, float maximum, float step);
int l2dcat_pref_control_combo(struct nk_context *context,
    const char *const *items, int count, int selected);
bool l2dcat_pref_controls_animating(struct nk_context *context);

#endif
