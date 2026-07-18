#ifndef L2DCAT_IMAGE_H
#define L2DCAT_IMAGE_H

#include "l2dcat/common.h"

typedef struct SDL_Surface SDL_Surface;

typedef struct L2DCatImage {
    unsigned char *pixels;
    int width;
    int height;
    SDL_Surface *surface;
} L2DCatImage;

L2DCatResult l2dcat_image_load(const char *path, L2DCatImage *image, L2DCatError *error);
void l2dcat_image_free(L2DCatImage *image);
unsigned int l2dcat_image_texture(const char *path, int *width, int *height,
    L2DCatError *error);
unsigned int l2dcat_image_composite_texture(const char *base, const char *left,
    const char *right, unsigned int texture, bool erase_left, bool erase_right,
    L2DCatError *error);

#endif
