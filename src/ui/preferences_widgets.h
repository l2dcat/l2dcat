#ifndef L2DCAT_PREFERENCES_WIDGETS_H
#define L2DCAT_PREFERENCES_WIDGETS_H

#include <stdbool.h>
#include "nuklear_config.h"

void l2dcat_pref_section(struct nk_context *context, const char *title);
bool l2dcat_pref_toggle(struct nk_context *context, const char *id,
    const char *title, const char *description, bool *value);
bool l2dcat_pref_float(struct nk_context *context, const char *id,
    const char *title, const char *description, float minimum, float *value,
    float maximum, float step);
bool l2dcat_pref_int(struct nk_context *context, const char *id,
    const char *title, const char *description, int minimum, int *value,
    int maximum, int step);
bool l2dcat_pref_slider(struct nk_context *context, const char *id,
    const char *title, const char *description, float minimum, float *value,
    float maximum, float step);
int l2dcat_pref_combo(struct nk_context *context, const char *id,
    const char *title, const char *description, const char *const *items,
    int count, int selected);
void l2dcat_pref_edit(struct nk_context *context, const char *id,
    const char *title, const char *description, char *value, int capacity);
bool l2dcat_pref_button(struct nk_context *context, const char *id,
    const char *title, const char *description, const char *button);
void l2dcat_pref_status(struct nk_context *context, const char *id,
    const char *title, const char *description);

#endif
