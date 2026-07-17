#ifndef BONGO_SHA256_H
#define BONGO_SHA256_H

#include "bongo/common.h"

void bongo_sha256_bytes(const void *data, size_t size, char output[65]);
BongoResult bongo_sha256_file(const char *path, char output[65], BongoError *error);

#endif
