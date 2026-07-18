#ifndef L2DCAT_OVERLAY_H
#define L2DCAT_OVERLAY_H

#include "l2dcat/common.h"

typedef struct L2DCatOverlay L2DCatOverlay;

L2DCatOverlay *l2dcat_overlay_create(L2DCatError *error);
void l2dcat_overlay_destroy(L2DCatOverlay *overlay);
L2DCatResult l2dcat_overlay_load(L2DCatOverlay *overlay, const char *model_directory,
    L2DCatError *error);
int l2dcat_overlay_key(L2DCatOverlay *overlay, const char *name, bool pressed);
bool l2dcat_overlay_hand_active(const L2DCatOverlay *overlay, bool right);
void l2dcat_overlay_begin_clip(L2DCatOverlay *overlay, float radius_percent);
void l2dcat_overlay_end_clip(L2DCatOverlay *overlay);
void l2dcat_overlay_draw_background(L2DCatOverlay *overlay, bool mirror);
void l2dcat_overlay_draw_keys(L2DCatOverlay *overlay, bool mirror);

#endif
