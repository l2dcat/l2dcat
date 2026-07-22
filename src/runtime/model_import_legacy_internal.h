#ifndef L2DCAT_MODEL_IMPORT_LEGACY_INTERNAL_H
#define L2DCAT_MODEL_IMPORT_LEGACY_INTERNAL_H

#include "model_import.h"

typedef struct L2DCatLegacyKeyNames {
    const char *items[2];
    char generated[16];
    size_t count;
} L2DCatLegacyKeyNames;

bool l2dcat_legacy_emit_pair(const char *hand, const char *keyboard,
    const char *directory, L2DCatLegacyKeyNames names, L2DCatError *error);

#endif
