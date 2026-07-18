#include "test.h"
#include "l2dcat/shortcut.h"

#include <string.h>

static L2DCatInputEvent key(L2DCatInputKind kind, const char *name) {
    L2DCatInputEvent event = {.kind = kind};
    snprintf(event.name, sizeof(event.name), "%s", name);
    return event;
}

void test_shortcut(void) {
    L2DCatShortcutState state;
    l2dcat_shortcut_init(&state);
    L2DCatInputEvent control = key(L2DCAT_INPUT_KEY_DOWN, "ControlLeft");
    L2DCatInputEvent letter = key(L2DCAT_INPUT_KEY_DOWN, "KeyB");
    CHECK(!l2dcat_shortcut_update(&state, &control));
    CHECK(l2dcat_shortcut_update(&state, &letter));
    CHECK(l2dcat_shortcut_matches(&state, &letter, "Control+B"));
    CHECK(!l2dcat_shortcut_matches(&state, &letter, "Control+Shift+B"));
    CHECK(!l2dcat_shortcut_update(&state, &letter));
    L2DCatInputEvent up = key(L2DCAT_INPUT_KEY_UP, "KeyB");
    l2dcat_shortcut_update(&state, &up);
    control.kind = L2DCAT_INPUT_KEY_UP;
    l2dcat_shortcut_update(&state, &control);
    L2DCatInputEvent function = key(L2DCAT_INPUT_KEY_DOWN, "F1");
    CHECK(l2dcat_shortcut_update(&state, &function));
    CHECK(l2dcat_shortcut_matches(&state, &function, "F1"));
    L2DCatInputEvent comma = key(L2DCAT_INPUT_KEY_DOWN, "Comma");
    up = key(L2DCAT_INPUT_KEY_UP, "F1");
    l2dcat_shortcut_update(&state, &up);
    control.kind = L2DCAT_INPUT_KEY_DOWN;
    l2dcat_shortcut_update(&state, &control);
    CHECK(l2dcat_shortcut_update(&state, &comma));
    CHECK(l2dcat_shortcut_matches(&state, &comma, "Control+Comma"));
}
