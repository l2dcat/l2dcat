#ifndef BONGO_INPUT_H
#define BONGO_INPUT_H

#include "bongo/common.h"
#include <stdatomic.h>

typedef enum BongoInputKind {
    BONGO_INPUT_NONE,
    BONGO_INPUT_KEY_DOWN,
    BONGO_INPUT_KEY_UP,
    BONGO_INPUT_MOUSE_DOWN,
    BONGO_INPUT_MOUSE_UP,
    BONGO_INPUT_MOUSE_MOVE,
    BONGO_INPUT_GAMEPAD_BUTTON,
    BONGO_INPUT_GAMEPAD_AXIS
} BongoInputKind;

typedef struct BongoInputEvent {
    BongoInputKind kind;
    uint64_t timestamp_ms;
    char name[BONGO_ID_CAP];
    float value;
    double x;
    double y;
} BongoInputEvent;

typedef struct BongoAutoRelease {
    uint64_t deadline_ms;
    char name[BONGO_ID_CAP];
} BongoAutoRelease;

typedef struct BongoInputState {
    BongoInputEvent queue[256];
    atomic_uint_fast16_t head;
    atomic_uint_fast16_t tail;
    _Atomic double mouse_x;
    _Atomic double mouse_y;
    atomic_bool mouse_dirty;
    atomic_uint_fast64_t dropped;
    BongoAutoRelease releases[BONGO_AUTO_RELEASE_CAP];
    size_t release_count;
} BongoInputState;

void bongo_input_init(BongoInputState *state);
bool bongo_input_push(BongoInputState *state, const BongoInputEvent *event);
bool bongo_input_pop(BongoInputState *state, BongoInputEvent *event);
void bongo_input_mouse(BongoInputState *state, double x, double y);
bool bongo_input_take_mouse(BongoInputState *state, double *x, double *y);
void bongo_input_auto_release(BongoInputState *state,
    const BongoInputEvent *event, uint64_t delay_ms);
bool bongo_input_take_release(BongoInputState *state, uint64_t now_ms,
    BongoInputEvent *event);

#endif
