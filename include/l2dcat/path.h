#ifndef L2DCAT_PATH_H
#define L2DCAT_PATH_H

#include "l2dcat/common.h"

#ifdef __cplusplus
extern "C" {
#endif

bool l2dcat_path_join(char *output, size_t capacity, const char *left, const char *right);
const char *l2dcat_path_name(const char *path);
bool l2dcat_path_is_file(const char *path);
bool l2dcat_path_is_dir(const char *path);
bool l2dcat_path_find_suffix(const char *directory, const char *suffix,
    char *name, size_t capacity);

#ifdef __cplusplus
}
#endif

#endif
