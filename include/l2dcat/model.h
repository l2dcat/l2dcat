#ifndef L2DCAT_MODEL_H
#define L2DCAT_MODEL_H

#include "l2dcat/config.h"

typedef struct L2DCatModelEntry {
    char id[L2DCAT_ID_CAP];
    char directory[L2DCAT_PATH_CAP];
    char setting_file[L2DCAT_PATH_CAP];
    L2DCatModelMode mode;
    bool preset;
} L2DCatModelEntry;

typedef struct L2DCatModelCatalog {
    L2DCatModelEntry entries[L2DCAT_MODEL_CAP];
    size_t count;
} L2DCatModelCatalog;

typedef enum L2DCatBehaviorKind {
    L2DCAT_BEHAVIOR_MOTION,
    L2DCAT_BEHAVIOR_EXPRESSION
} L2DCatBehaviorKind;

typedef struct L2DCatBehaviorEntry {
    char id[L2DCAT_PATH_CAP];
    char label[L2DCAT_ID_CAP];
    char group[L2DCAT_ID_CAP];
    char sound[L2DCAT_PATH_CAP];
    int index;
    L2DCatBehaviorKind kind;
} L2DCatBehaviorEntry;

typedef struct L2DCatBehaviorCatalog {
    L2DCatBehaviorEntry entries[L2DCAT_BEHAVIOR_CAP];
    size_t count;
} L2DCatBehaviorCatalog;

#ifdef __cplusplus
extern "C" {
#endif

void l2dcat_models_init(L2DCatModelCatalog *catalog);
L2DCatResult l2dcat_models_scan(L2DCatModelCatalog *catalog, const char *root,
    bool preset, L2DCatError *error);
const L2DCatModelEntry *l2dcat_models_find(const L2DCatModelCatalog *catalog,
    const char *id);
L2DCatResult l2dcat_behaviors_load(L2DCatBehaviorCatalog *catalog,
    const L2DCatModelEntry *model, L2DCatError *error);

typedef struct L2DCatLive2D L2DCatLive2D;
typedef struct L2DCatParameterRange { float minimum, maximum, value; } L2DCatParameterRange;

L2DCatLive2D *l2dcat_live2d_create(const char *asset_root, L2DCatError *error);
void l2dcat_live2d_destroy(L2DCatLive2D *live2d);
L2DCatResult l2dcat_live2d_load(L2DCatLive2D *live2d, const char *model_dir,
    const char *setting_file, L2DCatError *error);
void l2dcat_live2d_resize(L2DCatLive2D *live2d, int width, int height);
bool l2dcat_live2d_update(L2DCatLive2D *live2d, float delta_seconds);
void l2dcat_live2d_draw(L2DCatLive2D *live2d);
void l2dcat_live2d_set_mirror(L2DCatLive2D *live2d, bool mirror);
bool l2dcat_live2d_set_parameter(L2DCatLive2D *live2d, const char *id, float value);
bool l2dcat_live2d_parameter(L2DCatLive2D *live2d, const char *id,
    L2DCatParameterRange *range);
bool l2dcat_live2d_start_motion(L2DCatLive2D *live2d, const char *group, int index);
bool l2dcat_live2d_set_expression(L2DCatLive2D *live2d, int index);

#ifdef __cplusplus
}
#endif

#endif
