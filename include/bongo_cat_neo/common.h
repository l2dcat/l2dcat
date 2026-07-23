#ifndef BONGO_CAT_NEO_COMMON_H
#define BONGO_CAT_NEO_COMMON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define BONGO_CAT_NEO_NAME "Bongo Cat Neo"
#define BONGO_CAT_NEO_SLUG "bongo-cat-neo"
#define BONGO_CAT_NEO_EXECUTABLE "BongoCatNeo"
#define BONGO_CAT_NEO_APP_ID "com.bongocatneo.desktop"
#define BONGO_CAT_NEO_NAME_W L"Bongo Cat Neo"
#define BONGO_CAT_NEO_VERSION "0.1.0"
#define BONGO_CAT_NEO_PATH_CAP 1024
#define BONGO_CAT_NEO_ID_CAP 128
#define BONGO_CAT_NEO_SHORTCUT_CAP 128
#define BONGO_CAT_NEO_MODEL_CAP 128
#define BONGO_CAT_NEO_BEHAVIOR_CAP 128
#define BONGO_CAT_NEO_AUTO_RELEASE_CAP 64

typedef enum BongoCatNeoResult {
    BONGO_CAT_NEO_OK = 0,
    BONGO_CAT_NEO_ERROR_ARGUMENT,
    BONGO_CAT_NEO_ERROR_IO,
    BONGO_CAT_NEO_ERROR_FORMAT,
    BONGO_CAT_NEO_ERROR_MEMORY,
    BONGO_CAT_NEO_ERROR_PLATFORM,
    BONGO_CAT_NEO_ERROR_CUBISM
} BongoCatNeoResult;

typedef struct BongoCatNeoError {
    BongoCatNeoResult code;
    char message[256];
} BongoCatNeoError;

#ifdef __cplusplus
extern "C" {
#endif

void bongo_cat_neo_error_set(BongoCatNeoError *error, BongoCatNeoResult code, const char *format, ...);

#ifdef __cplusplus
}
#endif

#endif
