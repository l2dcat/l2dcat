#include "test.h"
#include "bongo/input.h"

#include <string.h>

void test_input(void) {
    BongoInputState state;
    bongo_input_init(&state);
    BongoInputEvent event = {.kind = BONGO_INPUT_KEY_DOWN, .value = 1.0f};
    strcpy(event.name, "KeyA");
    CHECK(bongo_input_push(&state, &event));
    BongoInputEvent output;
    CHECK(bongo_input_pop(&state, &output));
    CHECK(output.kind == BONGO_INPUT_KEY_DOWN);
    CHECK(strcmp(output.name, "KeyA") == 0);
    CHECK(!bongo_input_pop(&state, &output));

    bongo_input_mouse(&state, 1.0, 2.0);
    bongo_input_mouse(&state, 8.0, 9.0);
    double x, y;
    CHECK(bongo_input_take_mouse(&state, &x, &y));
    CHECK(x == 8.0 && y == 9.0);
    CHECK(!bongo_input_take_mouse(&state, &x, &y));

    for (int i = 0; i < 255; ++i) CHECK(bongo_input_push(&state, &event));
    CHECK(!bongo_input_push(&state, &event));
    CHECK(atomic_load(&state.dropped) == 1);

    bongo_input_init(&state);
    event.kind = BONGO_INPUT_KEY_DOWN;
    event.timestamp_ms = 100;
    strcpy(event.name, "KeyA");
    bongo_input_auto_release(&state, &event, 3000);
    CHECK(!bongo_input_take_release(&state, 3099, &output));
    event.timestamp_ms = 200;
    bongo_input_auto_release(&state, &event, 3000);
    CHECK(!bongo_input_take_release(&state, 3199, &output));
    CHECK(bongo_input_take_release(&state, 3200, &output));
    CHECK(output.kind == BONGO_INPUT_KEY_UP);
    CHECK(strcmp(output.name, "KeyA") == 0);
    bongo_input_auto_release(&state, &event, 3000);
    event.kind = BONGO_INPUT_KEY_UP;
    bongo_input_auto_release(&state, &event, 0);
    CHECK(!bongo_input_take_release(&state, 9999, &output));
}
