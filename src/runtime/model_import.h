#ifndef BONGO_CAT_NEO_MODEL_IMPORT_H
#define BONGO_CAT_NEO_MODEL_IMPORT_H

#include "runtime.h"

#define BONGO_CAT_NEO_IMPORT_CANDIDATE_CAP 16

typedef struct BongoCatNeoImportCandidate {
    char directory[BONGO_CAT_NEO_PATH_CAP];
    char setting[BONGO_CAT_NEO_PATH_CAP];
    char assets[BONGO_CAT_NEO_PATH_CAP];
    BongoCatNeoModelMode mode;
} BongoCatNeoImportCandidate;

typedef struct BongoCatNeoImportDiscovery {
    BongoCatNeoImportCandidate candidates[BONGO_CAT_NEO_IMPORT_CANDIDATE_CAP];
    size_t count;
    int depth;
} BongoCatNeoImportDiscovery;

bool bongo_cat_neo_import_discover(const char *source, BongoCatNeoImportDiscovery *discovery,
    BongoCatNeoError *error);
bool bongo_cat_neo_import_manifest_valid(const char *root, const char *setting,
    BongoCatNeoError *error);
bool bongo_cat_neo_import_legacy_assets(const BongoCatNeoImportCandidate *candidate,
    const char *source_root, const char *target, BongoCatNeoError *error);

#endif
