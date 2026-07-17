#ifndef BONGO_PATH_H
#define BONGO_PATH_H

#include "bongo/common.h"

bool bongo_path_join(char *output, size_t capacity, const char *left, const char *right);
const char *bongo_path_name(const char *path);
bool bongo_path_is_file(const char *path);
bool bongo_path_is_dir(const char *path);
bool bongo_path_find_suffix(const char *directory, const char *suffix,
    char *name, size_t capacity);

#endif
