#include "test.h"
#include "l2dcat/input.h"

#include <string.h>

void test_input(void) {
    L2DCatInputState state;
    l2dcat_input_init(&state);
    L2DCatInputEvent event = {.kind = L2DCAT_INPUT_KEY_DOWN, .value = 1.0f};
    strcpy(event.name, "KeyA");
    CHECK(l2dcat_input_push(&state, &event));
    L2DCatInputEvent output;
    CHECK(l2dcat_input_pop(&state, &output));
    CHECK(output.kind == L2DCAT_INPUT_KEY_DOWN);
    CHECK(strcmp(output.name, "KeyA") == 0);
    CHECK(!l2dcat_input_pop(&state, &output));

    l2dcat_input_mouse(&state, 1.0, 2.0);
    l2dcat_input_mouse(&state, 8.0, 9.0);
    double x, y;
    CHECK(l2dcat_input_take_mouse(&state, &x, &y));
    CHECK(x == 8.0 && y == 9.0);
    CHECK(!l2dcat_input_take_mouse(&state, &x, &y));

    for (int i = 0; i < 255; ++i) CHECK(l2dcat_input_push(&state, &event));
    CHECK(!l2dcat_input_push(&state, &event));
    CHECK(atomic_load(&state.dropped) == 1);

    l2dcat_input_init(&state);
    event.kind = L2DCAT_INPUT_KEY_DOWN;
    event.timestamp_ms = 100;
    strcpy(event.name, "KeyA");
    l2dcat_input_auto_release(&state, &event, 3000);
    CHECK(!l2dcat_input_take_release(&state, 3099, &output));
    event.timestamp_ms = 200;
    l2dcat_input_auto_release(&state, &event, 3000);
    CHECK(!l2dcat_input_take_release(&state, 3199, &output));
    CHECK(l2dcat_input_take_release(&state, 3200, &output));
    CHECK(output.kind == L2DCAT_INPUT_KEY_UP);
    CHECK(strcmp(output.name, "KeyA") == 0);
    l2dcat_input_auto_release(&state, &event, 3000);
    event.kind = L2DCAT_INPUT_KEY_UP;
    l2dcat_input_auto_release(&state, &event, 0);
    CHECK(!l2dcat_input_take_release(&state, 9999, &output));
}
