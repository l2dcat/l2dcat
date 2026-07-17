#include "bongo/model.h"

#include <stdlib.h>

struct BongoLive2D { int width; int height; };

BongoLive2D *bongo_live2d_create(BongoError *error) {
    BongoLive2D *value = calloc(1, sizeof(*value));
    if (!value) bongo_error_set(error, BONGO_ERROR_MEMORY, "Cannot allocate Live2D runtime");
    return value;
}

void bongo_live2d_destroy(BongoLive2D *live2d) { free(live2d); }

BongoResult bongo_live2d_load(BongoLive2D *live2d, const char *model_dir,
    const char *setting_file, BongoError *error) {
    (void)error;
    if (!live2d || !model_dir || !setting_file) return BONGO_ERROR_ARGUMENT;
    return BONGO_OK;
}

void bongo_live2d_resize(BongoLive2D *live2d, int width, int height) {
    if (!live2d) return;
    live2d->width = width;
    live2d->height = height;
}

bool bongo_live2d_update(BongoLive2D *live2d, float delta_seconds) {
    (void)live2d; (void)delta_seconds; return false;
}
void bongo_live2d_draw(BongoLive2D *live2d) { (void)live2d; }
void bongo_live2d_set_mirror(BongoLive2D *live2d, bool mirror) {
    (void)live2d; (void)mirror;
}
bool bongo_live2d_set_parameter(BongoLive2D *live2d, const char *id, float value) {
    (void)live2d; (void)id; (void)value; return false;
}
bool bongo_live2d_parameter(BongoLive2D *value, const char *id, BongoParameterRange *range) {
    (void)value; (void)id; (void)range; return false;
}
bool bongo_live2d_start_motion(BongoLive2D *value, const char *group, int index) {
    (void)value; (void)group; (void)index; return false;
}
bool bongo_live2d_set_expression(BongoLive2D *value, int index) {
    (void)value; (void)index; return false;
}
