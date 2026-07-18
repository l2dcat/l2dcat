#ifndef L2DCAT_COMMON_H
#define L2DCAT_COMMON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define L2DCAT_NAME "l2dcat"
#define L2DCAT_VERSION "1.1.0"
#define L2DCAT_PATH_CAP 1024
#define L2DCAT_ID_CAP 128
#define L2DCAT_SHORTCUT_CAP 128
#define L2DCAT_MODEL_CAP 128
#define L2DCAT_BEHAVIOR_CAP 128
#define L2DCAT_AUTO_RELEASE_CAP 64

typedef enum L2DCatResult {
    L2DCAT_OK = 0,
    L2DCAT_ERROR_ARGUMENT,
    L2DCAT_ERROR_IO,
    L2DCAT_ERROR_FORMAT,
    L2DCAT_ERROR_MEMORY,
    L2DCAT_ERROR_PLATFORM,
    L2DCAT_ERROR_CUBISM
} L2DCatResult;

typedef struct L2DCatError {
    L2DCatResult code;
    char message[256];
} L2DCatError;

void l2dcat_error_set(L2DCatError *error, L2DCatResult code, const char *format, ...);

#endif
