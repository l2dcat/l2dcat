#include "test.h"
#include "l2dcat/input.h"
#include "l2dcat/mouse.h"

#include <string.h>
#ifdef _WIN32
#include "../src/platform/windows_keys.h"
#endif

void test_input(void) {
#ifdef _WIN32
    char key_name[16];
    KBDLLHOOKSTRUCT key = {.vkCode = VK_NUMPAD1};
    CHECK(strcmp(l2dcat_windows_key_name(&key, key_name), "Kp1") == 0);
    key.vkCode = VK_END;
    CHECK(strcmp(l2dcat_windows_key_name(&key, key_name), "Kp1") == 0);
    key.flags = LLKHF_EXTENDED;
    CHECK(strcmp(l2dcat_windows_key_name(&key, key_name), "End") == 0);
    key.vkCode = VK_SNAPSHOT;
    CHECK(strcmp(l2dcat_windows_key_name(&key, key_name), "PrintScreen") == 0);
    key.vkCode = VK_APPS;
    CHECK(strcmp(l2dcat_windows_key_name(&key, key_name), "Apps") == 0);
#endif
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

    CHECK(l2dcat_input_mouse(&state, 1.0, 2.0));
    CHECK(!l2dcat_input_mouse(&state, 8.0, 9.0));
    double x, y;
    CHECK(l2dcat_input_take_mouse(&state, &x, &y));
    CHECK(x == 8.0 && y == 9.0);
    CHECK(!l2dcat_input_take_mouse(&state, &x, &y));
    CHECK(l2dcat_input_mouse(&state, 10.0, 11.0));
    CHECK(l2dcat_input_take_mouse(&state, &x, &y));
    CHECK(x == 10.0 && y == 11.0);

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

    l2dcat_input_init(&state);
    event.kind = L2DCAT_INPUT_KEY_DOWN;
    memcpy(event.name, "Fill", sizeof("Fill"));
    for (int i = 0; i < 255; ++i) CHECK(l2dcat_input_push(&state, &event));
    L2DCatInputEvent release = {.kind = L2DCAT_INPUT_KEY_UP, .timestamp_ms = 7};
    snprintf(release.name, sizeof(release.name), "KeyZ");
    CHECK(l2dcat_input_push(&state, &release));
    CHECK(l2dcat_input_pop(&state, &output));
    event.timestamp_ms = 7;
    memcpy(event.name, "KeyZ", sizeof("KeyZ"));
    CHECK(l2dcat_input_push(&state, &event));
    L2DCatInputKind key_z_order[2] = {0}; size_t key_z_count = 0;
    while (l2dcat_input_pop(&state, &output))
        if (strcmp(output.name, "KeyZ") == 0 && key_z_count < 2)
            key_z_order[key_z_count++] = output.kind;
    CHECK(key_z_count == 2 && key_z_order[0] == L2DCAT_INPUT_KEY_UP &&
        key_z_order[1] == L2DCAT_INPUT_KEY_DOWN);
    CHECK(atomic_load(&state.dropped) == 0);

    memcpy(event.name, "ControlLeft", sizeof("ControlLeft"));
    for (int i = 0; i < 255; ++i) CHECK(l2dcat_input_push(&state, &event));
    CHECK(!l2dcat_input_push(&state, &event));
    CHECK(l2dcat_input_control_down(&state));
    event.kind = L2DCAT_INPUT_KEY_UP;
    CHECK(l2dcat_input_push(&state, &event));
    CHECK(!l2dcat_input_control_down(&state));
    for (unsigned i = 1; i < L2DCAT_INPUT_RECOVERY_CAP - 1; ++i) {
        snprintf(release.name, sizeof(release.name), "Release%u", i);
        CHECK(l2dcat_input_push(&state, &release));
    }
    snprintf(release.name, sizeof(release.name), "RecoveryFull");
    CHECK(!l2dcat_input_push(&state, &release));
    CHECK(atomic_load(&state.dropped) == 2);

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
