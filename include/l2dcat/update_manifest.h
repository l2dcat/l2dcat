#ifndef L2DCAT_UPDATE_MANIFEST_H
#define L2DCAT_UPDATE_MANIFEST_H

#include "l2dcat/common.h"

typedef struct L2DCatUpdateManifest {
    char version[32];
    char notes[1024];
    char published[64];
    char url[L2DCAT_PATH_CAP];
    char sha256[65];
    char signature[129];
    uint64_t size;
} L2DCatUpdateManifest;

int l2dcat_version_compare(const char *left, const char *right);
L2DCatResult l2dcat_update_manifest_load(const char *path, const char *platform,
    L2DCatUpdateManifest *manifest, L2DCatError *error);

#endif
