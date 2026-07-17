#ifndef BONGO_MODEL_H
#define BONGO_MODEL_H

#include "bongo/config.h"

typedef struct BongoModelEntry {
    char id[BONGO_ID_CAP];
    char directory[BONGO_PATH_CAP];
    char setting_file[BONGO_PATH_CAP];
    BongoModelMode mode;
    bool preset;
} BongoModelEntry;

typedef struct BongoModelCatalog {
    BongoModelEntry entries[BONGO_MODEL_CAP];
    size_t count;
} BongoModelCatalog;

typedef enum BongoBehaviorKind {
    BONGO_BEHAVIOR_MOTION,
    BONGO_BEHAVIOR_EXPRESSION
} BongoBehaviorKind;

typedef struct BongoBehaviorEntry {
    char id[BONGO_PATH_CAP];
    char label[BONGO_ID_CAP];
    char group[BONGO_ID_CAP];
    char sound[BONGO_PATH_CAP];
    int index;
    BongoBehaviorKind kind;
} BongoBehaviorEntry;

typedef struct BongoBehaviorCatalog {
    BongoBehaviorEntry entries[BONGO_BEHAVIOR_CAP];
    size_t count;
} BongoBehaviorCatalog;

void bongo_models_init(BongoModelCatalog *catalog);
BongoResult bongo_models_scan(BongoModelCatalog *catalog, const char *root,
    bool preset, BongoError *error);
const BongoModelEntry *bongo_models_find(const BongoModelCatalog *catalog,
    const char *id);
BongoResult bongo_behaviors_load(BongoBehaviorCatalog *catalog,
    const BongoModelEntry *model, BongoError *error);

typedef struct BongoLive2D BongoLive2D;
typedef struct BongoParameterRange { float minimum, maximum, value; } BongoParameterRange;

BongoLive2D *bongo_live2d_create(BongoError *error);
void bongo_live2d_destroy(BongoLive2D *live2d);
BongoResult bongo_live2d_load(BongoLive2D *live2d, const char *model_dir,
    const char *setting_file, BongoError *error);
void bongo_live2d_resize(BongoLive2D *live2d, int width, int height);
bool bongo_live2d_update(BongoLive2D *live2d, float delta_seconds);
void bongo_live2d_draw(BongoLive2D *live2d);
void bongo_live2d_set_mirror(BongoLive2D *live2d, bool mirror);
bool bongo_live2d_set_parameter(BongoLive2D *live2d, const char *id, float value);
bool bongo_live2d_parameter(BongoLive2D *live2d, const char *id,
    BongoParameterRange *range);
bool bongo_live2d_start_motion(BongoLive2D *live2d, const char *group, int index);
bool bongo_live2d_set_expression(BongoLive2D *live2d, int index);

#endif
