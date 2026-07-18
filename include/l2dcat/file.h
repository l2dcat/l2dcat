#ifndef L2DCAT_FILE_H
#define L2DCAT_FILE_H

#include "l2dcat/common.h"
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

FILE *l2dcat_file_open(const char *path, const char *mode);
bool l2dcat_file_remove(const char *path);
bool l2dcat_file_replace(const char *source, const char *target, bool durable);

#ifdef __cplusplus
}
#endif

#endif
