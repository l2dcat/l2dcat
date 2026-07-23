#ifndef BONGO_CAT_NEO_SHA256_H
#define BONGO_CAT_NEO_SHA256_H

#include "bongo_cat_neo/common.h"

void bongo_cat_neo_sha256_bytes(const void *data, size_t size, char output[65]);
BongoCatNeoResult bongo_cat_neo_sha256_file(const char *path, char output[65], BongoCatNeoError *error);

#endif
