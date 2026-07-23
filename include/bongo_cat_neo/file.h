#ifndef BONGO_CAT_NEO_FILE_H
#define BONGO_CAT_NEO_FILE_H

#include "bongo_cat_neo/common.h"
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

FILE *bongo_cat_neo_file_open(const char *path, const char *mode);
bool bongo_cat_neo_file_remove(const char *path);
bool bongo_cat_neo_file_replace(const char *source, const char *target, bool durable);

#ifdef __cplusplus
}
#endif

#endif
