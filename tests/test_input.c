#include "test.h"
#include "l2dcat/input.h"
#include "l2dcat/mouse.h"

#include <string.h>

void test_input(void) {
    L2DCatInputState state;
    l2dcat_input_init(&state);
    L2DCatInputEvent event = {.kind = L2DCAT_INPUT_KEY_DOWN, .value = 1.0f};
    memcpy(event.name, "KeyA", sizeof("KeyA"));
    CHECK(l2dcat_input_push(&state, &event));
    L2DCatInputEvent output;
    CHECK(l2dcat_input_pop(&state, &output));
    CHECK(output.kind == L2DCAT_INPUT_KEY_DOWN);

    memcpy(event.name, "ControlLeft", sizeof("ControlLeft"));
    event.kind = L2DCAT_INPUT_KEY_DOWN;
    CHECK(l2dcat_input_push(&state, &event));
    CHECK(l2dcat_input_control_down(&state));
    event.kind = L2DCAT_INPUT_KEY_UP;
    CHECK(l2dcat_input_push(&state, &event));
    CHECK(!l2dcat_input_control_down(&state));
    CHECK(strcmp(output.name, "KeyA") == 0);
    CHECK(l2dcat_input_pop(&state, &output));
    CHECK(output.kind == L2DCAT_INPUT_KEY_DOWN);
    CHECK(l2dcat_input_pop(&state, &output));
    CHECK(output.kind == L2DCAT_INPUT_KEY_UP);
    CHECK(!l2dcat_input_pop(&state, &output));

    memcpy(event.name, "ShiftRight", sizeof("ShiftRight"));
    event.kind = L2DCAT_INPUT_KEY_DOWN;
    CHECK(l2dcat_input_push(&state, &event));
    CHECK(l2dcat_input_shift_down(&state));
    event.kind = L2DCAT_INPUT_KEY_UP;
    CHECK(l2dcat_input_push(&state, &event));
    CHECK(!l2dcat_input_shift_down(&state));
    CHECK(l2dcat_input_pop(&state, &output));
    CHECK(l2dcat_input_pop(&state, &output));

    l2dcat_input_mouse(&state, 1.0, 2.0);
    l2dcat_input_mouse(&state, 8.0, 9.0);
    double x, y;
    CHECK(l2dcat_input_take_mouse(&state, &x, &y));
    CHECK(x == 8.0 && y == 9.0);
    CHECK(!l2dcat_input_take_mouse(&state, &x, &y));

    L2DCatMouseTracking tracking = {0};
    l2dcat_mouse_target(&tracking, 0.0, 0.0);
    CHECK(l2dcat_mouse_step(&tracking, 1.0f / 60.0f, &x, &y));
    CHECK(x == 0.0 && y == 0.0);
    l2dcat_mouse_target(&tracking, 100.0, 40.0);
    CHECK(l2dcat_mouse_step(&tracking, 1.0f / 60.0f, &x, &y));
    CHECK(x > 24.99 && x < 25.01 && y > 9.99 && y < 10.01);
    for (int i = 0; i < 64 && !tracking.settled; ++i)
        CHECK(l2dcat_mouse_step(&tracking, 1.0f / 60.0f, &x, &y));
    CHECK(tracking.settled && x == 100.0 && y == 40.0);
    CHECK(!l2dcat_mouse_step(&tracking, 1.0f / 60.0f, &x, &y));
    CHECK(l2dcat_mouse_parameter_value(-30.0f, 30.0f,
        0.0f, 0.5f, 'X', false) == 30.0f);
    CHECK(l2dcat_mouse_parameter_value(-30.0f, 30.0f,
        1.0f, 0.5f, 'X', false) == -30.0f);
    CHECK(l2dcat_mouse_parameter_value(-30.0f, 30.0f,
        0.0f, 0.5f, 'X', true) == -30.0f);
    CHECK(l2dcat_mouse_parameter_value(-30.0f, 30.0f,
        0.5f, 0.0f, 'Y', true) == 30.0f);
    CHECK(l2dcat_mouse_parameter_value(-30.0f, 30.0f,
        0.0f, 0.0f, 'Z', false) == -30.0f);

    memcpy(event.name, "ControlLeft", sizeof("ControlLeft"));
    for (int i = 0; i < 255; ++i) CHECK(l2dcat_input_push(&state, &event));
    CHECK(!l2dcat_input_push(&state, &event));
    event.kind = L2DCAT_INPUT_KEY_DOWN;
    CHECK(!l2dcat_input_push(&state, &event));
    CHECK(l2dcat_input_control_down(&state));
    event.kind = L2DCAT_INPUT_KEY_UP;
    CHECK(!l2dcat_input_push(&state, &event));
    CHECK(!l2dcat_input_control_down(&state));
    memcpy(event.name, "ShiftLeft", sizeof("ShiftLeft"));
    event.kind = L2DCAT_INPUT_KEY_DOWN;
    CHECK(!l2dcat_input_push(&state, &event));
    CHECK(l2dcat_input_shift_down(&state));
    event.kind = L2DCAT_INPUT_KEY_UP;
    CHECK(!l2dcat_input_push(&state, &event));
    CHECK(!l2dcat_input_shift_down(&state));
    CHECK(atomic_load(&state.dropped) == 5);

    l2dcat_input_init(&state);
    event.kind = L2DCAT_INPUT_KEY_DOWN;
    event.timestamp_ms = 100;
    memcpy(event.name, "KeyA", sizeof("KeyA"));
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
