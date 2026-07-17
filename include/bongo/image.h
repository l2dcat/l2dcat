#ifndef BONGO_IMAGE_H
#define BONGO_IMAGE_H

#include "bongo/common.h"

typedef struct SDL_Surface SDL_Surface;

typedef struct BongoImage {
    unsigned char *pixels;
    int width;
    int height;
    SDL_Surface *surface;
} BongoImage;

BongoResult bongo_image_load(const char *path, BongoImage *image, BongoError *error);
void bongo_image_free(BongoImage *image);
unsigned int bongo_image_texture(const char *path, int *width, int *height,
    BongoError *error);
unsigned int bongo_image_composite_texture(const char *base, const char *left,
    const char *right, unsigned int texture, bool erase_left, bool erase_right,
    BongoError *error);

#endif
