#include "l2dcat/input.h"

#include <stdio.h>
#include <string.h>

void l2dcat_input_init(L2DCatInputState *state) {
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

bool l2dcat_input_push(L2DCatInputState *state, const L2DCatInputEvent *event) {
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

bool l2dcat_input_pop(L2DCatInputState *state, L2DCatInputEvent *event) {
    if (!state || !event) return false;
    uint16_t tail = (uint16_t)atomic_load_explicit(&state->tail, memory_order_relaxed);
    if (tail == atomic_load_explicit(&state->head, memory_order_acquire)) return false;
    *event = state->queue[tail];
    atomic_store_explicit(&state->tail, (tail + 1u) % 256u, memory_order_release);
    return true;
}

void l2dcat_input_mouse(L2DCatInputState *state, double x, double y) {
    if (!state) return;
    atomic_store_explicit(&state->mouse_x, x, memory_order_relaxed);
    atomic_store_explicit(&state->mouse_y, y, memory_order_relaxed);
    atomic_store_explicit(&state->mouse_dirty, true, memory_order_release);
}

bool l2dcat_input_take_mouse(L2DCatInputState *state, double *x, double *y) {
    if (!state || !x || !y ||
        !atomic_exchange_explicit(&state->mouse_dirty, false, memory_order_acquire)) return false;
    *x = atomic_load_explicit(&state->mouse_x, memory_order_relaxed);
    *y = atomic_load_explicit(&state->mouse_y, memory_order_relaxed);
    return true;
}

static size_t release_index(const L2DCatInputState *state, const char *name) {
    for (size_t i = 0; i < state->release_count; ++i)
        if (strcmp(state->releases[i].name, name) == 0) return i;
    return state->release_count;
}

static void remove_release(L2DCatInputState *state, size_t index) {
    if (index >= state->release_count) return;
    state->releases[index] = state->releases[state->release_count - 1];
    memset(&state->releases[--state->release_count], 0,
        sizeof(state->releases[0]));
}

void l2dcat_input_auto_release(L2DCatInputState *state,
    const L2DCatInputEvent *event, uint64_t delay_ms) {
    if (!state || !event || !event->name[0] ||
        (event->kind != L2DCAT_INPUT_KEY_DOWN && event->kind != L2DCAT_INPUT_KEY_UP)) return;
    size_t index = release_index(state, event->name);
    if (event->kind == L2DCAT_INPUT_KEY_UP || !delay_ms) {
        remove_release(state, index);
        return;
    }
    if (index == state->release_count) {
        if (state->release_count >= L2DCAT_AUTO_RELEASE_CAP) return;
        index = state->release_count++;
        snprintf(state->releases[index].name,
            sizeof(state->releases[index].name), "%s", event->name);
    }
    state->releases[index].deadline_ms = event->timestamp_ms + delay_ms;
}

bool l2dcat_input_take_release(L2DCatInputState *state, uint64_t now_ms,
    L2DCatInputEvent *event) {
    if (!state || !event) return false;
    for (size_t i = 0; i < state->release_count; ++i) {
        if (state->releases[i].deadline_ms > now_ms) continue;
        memset(event, 0, sizeof(*event));
        event->kind = L2DCAT_INPUT_KEY_UP;
        event->timestamp_ms = now_ms;
        snprintf(event->name, sizeof(event->name), "%s", state->releases[i].name);
        remove_release(state, i);
        return true;
    }
    return false;
}
