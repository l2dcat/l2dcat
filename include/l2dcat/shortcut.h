#ifndef L2DCAT_SHORTCUT_H
#define L2DCAT_SHORTCUT_H

#include "l2dcat/input.h"

typedef struct L2DCatShortcutState {
    uint8_t control;
    uint8_t shift;
    uint8_t alt;
    uint8_t meta;
    char pressed[L2DCAT_ID_CAP];
} L2DCatShortcutState;

void l2dcat_shortcut_init(L2DCatShortcutState *state);
bool l2dcat_shortcut_update(L2DCatShortcutState *state, const L2DCatInputEvent *event);
bool l2dcat_shortcut_matches(const L2DCatShortcutState *state,
    const L2DCatInputEvent *event, const char *shortcut);

#endif
