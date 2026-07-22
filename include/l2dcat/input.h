#ifndef L2DCAT_INPUT_H
#define L2DCAT_INPUT_H

#include "l2dcat/common.h"
#include <stdatomic.h>

#define L2DCAT_INPUT_QUEUE_CAP 256u
#define L2DCAT_INPUT_RECOVERY_CAP 65u

typedef enum L2DCatInputKind {
    L2DCAT_INPUT_NONE,
    L2DCAT_INPUT_KEY_DOWN,
    L2DCAT_INPUT_KEY_UP,
    L2DCAT_INPUT_MOUSE_DOWN,
    L2DCAT_INPUT_MOUSE_UP,
    L2DCAT_INPUT_MOUSE_MOVE,
    L2DCAT_INPUT_GAMEPAD_BUTTON,
    L2DCAT_INPUT_GAMEPAD_AXIS
} L2DCatInputKind;

typedef struct L2DCatInputEvent {
    L2DCatInputKind kind;
    uint64_t timestamp_ms;
    char name[L2DCAT_ID_CAP];
    float value;
    double x;
    double y;
    uint64_t sequence;
} L2DCatInputEvent;

typedef struct L2DCatAutoRelease {
    uint64_t deadline_ms;
    char name[L2DCAT_ID_CAP];
} L2DCatAutoRelease;

typedef struct L2DCatInputState {
    L2DCatInputEvent queue[L2DCAT_INPUT_QUEUE_CAP];
    atomic_uint_fast16_t head;
    atomic_uint_fast16_t tail;
    _Atomic double mouse_x;
    _Atomic double mouse_y;
    atomic_bool mouse_dirty;
    atomic_uint_fast8_t control;
    atomic_uint_fast8_t shift;
    atomic_uint_fast64_t dropped;
    atomic_uint_fast64_t next_sequence;
    L2DCatInputEvent recovery[L2DCAT_INPUT_RECOVERY_CAP];
    atomic_uint_fast8_t recovery_head;
    atomic_uint_fast8_t recovery_tail;
    L2DCatAutoRelease releases[L2DCAT_AUTO_RELEASE_CAP];
    size_t release_count;
} L2DCatInputState;

void l2dcat_input_init(L2DCatInputState *state);
bool l2dcat_input_push(L2DCatInputState *state, const L2DCatInputEvent *event);
bool l2dcat_input_pop(L2DCatInputState *state, L2DCatInputEvent *event);
bool l2dcat_input_mouse(L2DCatInputState *state, double x, double y);
bool l2dcat_input_take_mouse(L2DCatInputState *state, double *x, double *y);
bool l2dcat_input_control_down(const L2DCatInputState *state);
bool l2dcat_input_shift_down(const L2DCatInputState *state);
void l2dcat_input_auto_release(L2DCatInputState *state,
    const L2DCatInputEvent *event, uint64_t delay_ms);
bool l2dcat_input_take_release(L2DCatInputState *state, uint64_t now_ms,
    L2DCatInputEvent *event);

#endif
