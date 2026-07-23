#ifndef BONGO_CAT_NEO_MODEL_IMPORT_LEGACY_INTERNAL_H
#define BONGO_CAT_NEO_MODEL_IMPORT_LEGACY_INTERNAL_H

#include "model_import.h"

typedef struct BongoCatNeoLegacyKeyNames {
    const char *items[2];
    char generated[16];
    size_t count;
} BongoCatNeoLegacyKeyNames;

bool bongo_cat_neo_legacy_emit_pair(const char *hand, const char *keyboard,
    const char *directory, BongoCatNeoLegacyKeyNames names, BongoCatNeoError *error);

#endif
