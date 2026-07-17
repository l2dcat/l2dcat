#ifndef BONGO_PREFERENCES_WIDGETS_H
#define BONGO_PREFERENCES_WIDGETS_H

#include <stdbool.h>
#include "nuklear_config.h"

void bongo_pref_section(struct nk_context *context, const char *title);
bool bongo_pref_toggle(struct nk_context *context, const char *id,
    const char *title, const char *description, bool *value);
bool bongo_pref_float(struct nk_context *context, const char *id,
    const char *title, const char *description, float minimum, float *value,
    float maximum, float step);
bool bongo_pref_int(struct nk_context *context, const char *id,
    const char *title, const char *description, int minimum, int *value,
    int maximum, int step);
bool bongo_pref_slider(struct nk_context *context, const char *id,
    const char *title, const char *description, float minimum, float *value,
    float maximum, float step);
int bongo_pref_combo(struct nk_context *context, const char *id,
    const char *title, const char *description, const char *const *items,
    int count, int selected);
void bongo_pref_edit(struct nk_context *context, const char *id,
    const char *title, const char *description, char *value, int capacity);
bool bongo_pref_button(struct nk_context *context, const char *id,
    const char *title, const char *description, const char *button);
void bongo_pref_status(struct nk_context *context, const char *id,
    const char *title, const char *description);

#endif
