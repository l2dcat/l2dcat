#include "l2dcat/model.h"

#include <stdlib.h>

struct L2DCatLive2D { int width; int height; };

L2DCatLive2D *l2dcat_live2d_create(const char *asset_root, L2DCatError *error) {
    (void)asset_root;
    L2DCatLive2D *value = calloc(1, sizeof(*value));
    if (!value) l2dcat_error_set(error, L2DCAT_ERROR_MEMORY, "Cannot allocate Live2D runtime");
    return value;
}

void l2dcat_live2d_destroy(L2DCatLive2D *live2d) { free(live2d); }

L2DCatResult l2dcat_live2d_load(L2DCatLive2D *live2d, const char *model_dir,
    const char *setting_file, L2DCatError *error) {
    (void)error;
    if (!live2d || !model_dir || !setting_file) return L2DCAT_ERROR_ARGUMENT;
    return L2DCAT_OK;
}

void l2dcat_live2d_resize(L2DCatLive2D *live2d, int width, int height) {
    if (!live2d) return;
    live2d->width = width;
    live2d->height = height;
}

bool l2dcat_live2d_update(L2DCatLive2D *live2d, float delta_seconds) {
    (void)live2d; (void)delta_seconds; return false;
}
void l2dcat_live2d_draw(L2DCatLive2D *live2d) { (void)live2d; }
void l2dcat_live2d_set_mirror(L2DCatLive2D *live2d, bool mirror) {
    (void)live2d; (void)mirror;
}
bool l2dcat_live2d_set_parameter(L2DCatLive2D *live2d, const char *id, float value) {
    (void)live2d; (void)id; (void)value; return false;
}
bool l2dcat_live2d_parameter(L2DCatLive2D *value, const char *id, L2DCatParameterRange *range) {
    (void)value; (void)id; (void)range; return false;
}
bool l2dcat_live2d_start_motion(L2DCatLive2D *value, const char *group, int index) {
    (void)value; (void)group; (void)index; return false;
}
bool l2dcat_live2d_set_expression(L2DCatLive2D *value, int index) {
    (void)value; (void)index; return false;
}
