#ifndef BONGO_CAT_NEO_MOUSE_H
#define BONGO_CAT_NEO_MOUSE_H

#include <stdbool.h>

typedef struct BongoCatNeoMouseTracking {
    double target_x;
    double target_y;
    double current_x;
    double current_y;
    bool initialized;
    bool settled;
} BongoCatNeoMouseTracking;

void bongo_cat_neo_mouse_target(BongoCatNeoMouseTracking *tracking, double x, double y);
bool bongo_cat_neo_mouse_step(BongoCatNeoMouseTracking *tracking, float delta_seconds,
    double *x, double *y);
float bongo_cat_neo_mouse_parameter_value(float minimum, float maximum,
    float x_ratio, float y_ratio, char axis, bool mirror);

#endif
