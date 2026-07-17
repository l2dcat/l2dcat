#include "test.h"
#include "bongo/shortcut.h"

#include <string.h>

static BongoInputEvent key(BongoInputKind kind, const char *name) {
    BongoInputEvent event = {.kind = kind};
    snprintf(event.name, sizeof(event.name), "%s", name);
    return event;
}

void test_shortcut(void) {
    BongoShortcutState state;
    bongo_shortcut_init(&state);
    BongoInputEvent control = key(BONGO_INPUT_KEY_DOWN, "ControlLeft");
    BongoInputEvent letter = key(BONGO_INPUT_KEY_DOWN, "KeyB");
    CHECK(!bongo_shortcut_update(&state, &control));
    CHECK(bongo_shortcut_update(&state, &letter));
    CHECK(bongo_shortcut_matches(&state, &letter, "Control+B"));
    CHECK(!bongo_shortcut_matches(&state, &letter, "Control+Shift+B"));
    CHECK(!bongo_shortcut_update(&state, &letter));
    BongoInputEvent up = key(BONGO_INPUT_KEY_UP, "KeyB");
    bongo_shortcut_update(&state, &up);
    control.kind = BONGO_INPUT_KEY_UP;
    bongo_shortcut_update(&state, &control);
    BongoInputEvent function = key(BONGO_INPUT_KEY_DOWN, "F1");
    CHECK(bongo_shortcut_update(&state, &function));
    CHECK(bongo_shortcut_matches(&state, &function, "F1"));
    BongoInputEvent comma = key(BONGO_INPUT_KEY_DOWN, "Comma");
    up = key(BONGO_INPUT_KEY_UP, "F1");
    bongo_shortcut_update(&state, &up);
    control.kind = BONGO_INPUT_KEY_DOWN;
    bongo_shortcut_update(&state, &control);
    CHECK(bongo_shortcut_update(&state, &comma));
    CHECK(bongo_shortcut_matches(&state, &comma, "Control+Comma"));
}
