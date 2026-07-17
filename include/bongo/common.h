#ifndef BONGO_COMMON_H
#define BONGO_COMMON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define BONGO_NAME "BongoCat"
#define BONGO_VERSION "1.1.0"
#define BONGO_PATH_CAP 1024
#define BONGO_ID_CAP 128
#define BONGO_SHORTCUT_CAP 128
#define BONGO_MODEL_CAP 128
#define BONGO_BEHAVIOR_CAP 128
#define BONGO_AUTO_RELEASE_CAP 64

typedef enum BongoResult {
    BONGO_OK = 0,
    BONGO_ERROR_ARGUMENT,
    BONGO_ERROR_IO,
    BONGO_ERROR_FORMAT,
    BONGO_ERROR_MEMORY,
    BONGO_ERROR_PLATFORM,
    BONGO_ERROR_CUBISM
} BongoResult;

typedef struct BongoError {
    BongoResult code;
    char message[256];
} BongoError;

void bongo_error_set(BongoError *error, BongoResult code, const char *format, ...);

#endif
