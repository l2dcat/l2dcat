#ifndef L2DCAT_MODEL_IMPORT_H
#define L2DCAT_MODEL_IMPORT_H

#include "runtime.h"

#define L2DCAT_IMPORT_CANDIDATE_CAP 16

typedef struct L2DCatImportCandidate {
    char directory[L2DCAT_PATH_CAP];
    char setting[L2DCAT_PATH_CAP];
    char assets[L2DCAT_PATH_CAP];
    L2DCatModelMode mode;
} L2DCatImportCandidate;

typedef struct L2DCatImportDiscovery {
    L2DCatImportCandidate candidates[L2DCAT_IMPORT_CANDIDATE_CAP];
    size_t count;
    int depth;
} L2DCatImportDiscovery;

bool l2dcat_import_discover(const char *source, L2DCatImportDiscovery *discovery,
    L2DCatError *error);
bool l2dcat_import_manifest_valid(const char *root, const char *setting,
    L2DCatError *error);

#endif
