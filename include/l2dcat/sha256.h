#ifndef L2DCAT_SHA256_H
#define L2DCAT_SHA256_H

#include "l2dcat/common.h"

void l2dcat_sha256_bytes(const void *data, size_t size, char output[65]);
L2DCatResult l2dcat_sha256_file(const char *path, char output[65], L2DCatError *error);

#endif
