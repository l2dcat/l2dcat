#ifndef L2DCAT_MOUSE_H
#define L2DCAT_MOUSE_H

#include <stdbool.h>

typedef struct L2DCatMouseTracking {
    double target_x;
    double target_y;
    double current_x;
    double current_y;
    bool initialized;
    bool settled;
} L2DCatMouseTracking;

void l2dcat_mouse_target(L2DCatMouseTracking *tracking, double x, double y);
bool l2dcat_mouse_step(L2DCatMouseTracking *tracking, float delta_seconds,
    double *x, double *y);
float l2dcat_mouse_parameter_value(float minimum, float maximum,
    float x_ratio, float y_ratio, char axis, bool mirror);

#endif
