#ifndef BONGO_UPDATE_MANIFEST_H
#define BONGO_UPDATE_MANIFEST_H

#include "bongo/common.h"

typedef struct BongoUpdateManifest {
    char version[32];
    char notes[1024];
    char published[64];
    char url[BONGO_PATH_CAP];
    char sha256[65];
    char signature[129];
    uint64_t size;
} BongoUpdateManifest;

int bongo_version_compare(const char *left, const char *right);
BongoResult bongo_update_manifest_load(const char *path, const char *platform,
    BongoUpdateManifest *manifest, BongoError *error);

#endif
