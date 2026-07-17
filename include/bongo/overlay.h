#ifndef BONGO_OVERLAY_H
#define BONGO_OVERLAY_H

#include "bongo/common.h"

typedef struct BongoOverlay BongoOverlay;

BongoOverlay *bongo_overlay_create(BongoError *error);
void bongo_overlay_destroy(BongoOverlay *overlay);
BongoResult bongo_overlay_load(BongoOverlay *overlay, const char *model_directory,
    BongoError *error);
int bongo_overlay_key(BongoOverlay *overlay, const char *name, bool pressed);
bool bongo_overlay_hand_active(const BongoOverlay *overlay, bool right);
void bongo_overlay_begin_clip(BongoOverlay *overlay, float radius_percent);
void bongo_overlay_end_clip(BongoOverlay *overlay);
void bongo_overlay_draw_background(BongoOverlay *overlay, bool mirror);
void bongo_overlay_draw_keys(BongoOverlay *overlay, bool mirror);

#endif
