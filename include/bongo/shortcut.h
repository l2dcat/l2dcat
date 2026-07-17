#ifndef BONGO_SHORTCUT_H
#define BONGO_SHORTCUT_H

#include "bongo/input.h"

typedef struct BongoShortcutState {
    uint8_t control;
    uint8_t shift;
    uint8_t alt;
    uint8_t meta;
    char pressed[BONGO_ID_CAP];
} BongoShortcutState;

void bongo_shortcut_init(BongoShortcutState *state);
bool bongo_shortcut_update(BongoShortcutState *state, const BongoInputEvent *event);
bool bongo_shortcut_matches(const BongoShortcutState *state,
    const BongoInputEvent *event, const char *shortcut);

#endif
