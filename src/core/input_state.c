#include "bongo/input.h"

#include <stdio.h>
#include <string.h>

void bongo_input_init(BongoInputState *state) {
    if (!state) return;
    memset(state->queue, 0, sizeof(state->queue));
    atomic_init(&state->head, 0);
    atomic_init(&state->tail, 0);
    atomic_init(&state->mouse_x, 0.0);
    atomic_init(&state->mouse_y, 0.0);
    atomic_init(&state->mouse_dirty, false);
    atomic_init(&state->dropped, 0);
    memset(state->releases, 0, sizeof(state->releases));
    state->release_count = 0;
}

bool bongo_input_push(BongoInputState *state, const BongoInputEvent *event) {
    if (!state || !event) return false;
    uint16_t head = (uint16_t)atomic_load_explicit(&state->head, memory_order_relaxed);
    uint16_t next = (uint16_t)((head + 1u) % 256u);
    if (next == atomic_load_explicit(&state->tail, memory_order_acquire)) {
        atomic_fetch_add_explicit(&state->dropped, 1, memory_order_relaxed);
        return false;
    }
    state->queue[head] = *event;
    atomic_store_explicit(&state->head, next, memory_order_release);
    return true;
}

bool bongo_input_pop(BongoInputState *state, BongoInputEvent *event) {
    if (!state || !event) return false;
    uint16_t tail = (uint16_t)atomic_load_explicit(&state->tail, memory_order_relaxed);
    if (tail == atomic_load_explicit(&state->head, memory_order_acquire)) return false;
    *event = state->queue[tail];
    atomic_store_explicit(&state->tail, (tail + 1u) % 256u, memory_order_release);
    return true;
}

void bongo_input_mouse(BongoInputState *state, double x, double y) {
    if (!state) return;
    atomic_store_explicit(&state->mouse_x, x, memory_order_relaxed);
    atomic_store_explicit(&state->mouse_y, y, memory_order_relaxed);
    atomic_store_explicit(&state->mouse_dirty, true, memory_order_release);
}

bool bongo_input_take_mouse(BongoInputState *state, double *x, double *y) {
    if (!state || !x || !y ||
        !atomic_exchange_explicit(&state->mouse_dirty, false, memory_order_acquire)) return false;
    *x = atomic_load_explicit(&state->mouse_x, memory_order_relaxed);
    *y = atomic_load_explicit(&state->mouse_y, memory_order_relaxed);
    return true;
}

static size_t release_index(const BongoInputState *state, const char *name) {
    for (size_t i = 0; i < state->release_count; ++i)
        if (strcmp(state->releases[i].name, name) == 0) return i;
    return state->release_count;
}

static void remove_release(BongoInputState *state, size_t index) {
    if (index >= state->release_count) return;
    state->releases[index] = state->releases[state->release_count - 1];
    memset(&state->releases[--state->release_count], 0,
        sizeof(state->releases[0]));
}

void bongo_input_auto_release(BongoInputState *state,
    const BongoInputEvent *event, uint64_t delay_ms) {
    if (!state || !event || !event->name[0] ||
        (event->kind != BONGO_INPUT_KEY_DOWN && event->kind != BONGO_INPUT_KEY_UP)) return;
    size_t index = release_index(state, event->name);
    if (event->kind == BONGO_INPUT_KEY_UP || !delay_ms) {
        remove_release(state, index);
        return;
    }
    if (index == state->release_count) {
        if (state->release_count >= BONGO_AUTO_RELEASE_CAP) return;
        index = state->release_count++;
        snprintf(state->releases[index].name,
            sizeof(state->releases[index].name), "%s", event->name);
    }
    state->releases[index].deadline_ms = event->timestamp_ms + delay_ms;
}

bool bongo_input_take_release(BongoInputState *state, uint64_t now_ms,
    BongoInputEvent *event) {
    if (!state || !event) return false;
    for (size_t i = 0; i < state->release_count; ++i) {
        if (state->releases[i].deadline_ms > now_ms) continue;
        memset(event, 0, sizeof(*event));
        event->kind = BONGO_INPUT_KEY_UP;
        event->timestamp_ms = now_ms;
        snprintf(event->name, sizeof(event->name), "%s", state->releases[i].name);
        remove_release(state, i);
        return true;
    }
    return false;
}
