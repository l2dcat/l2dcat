#include "runtime.h"
#include "bongo/file.h"
#include "bongo/path.h"

#include <SDL3/SDL_opengl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void bongo_frame_audit(BongoApp *app, int width, int height) {
    if (!app || !app->smoke || width < 2 || height < 2) return;
    char path[BONGO_PATH_CAP];
    if (!app->smoke_frame_audited) {
        app->smoke_frame_audited = true;
        const int points[][2] = {{0, 0}, {width - 1, 0}, {0, height - 1},
            {width - 1, height - 1}, {width / 2, height / 2}};
        unsigned transparent = 0, opaque = 0;
        for (size_t i = 0; i < sizeof(points) / sizeof(points[0]); ++i) {
            unsigned char pixel[4] = {0};
            glReadPixels(points[i][0], points[i][1], 1, 1, GL_RGBA,
                GL_UNSIGNED_BYTE, pixel);
            if (pixel[3] < 16) transparent++;
            if (pixel[3] > 239) opaque++;
        }
        bongo_path_join(path, sizeof(path), app->data_root, "frame-alpha.txt");
        FILE *file = bongo_file_open(path, "wb");
        if (file) {
            fprintf(file, "samples=5 transparent=%u opaque=%u gl_error=%u\n",
                transparent, opaque, (unsigned)glGetError());
            fclose(file);
        }
    }
    size_t pitch = (size_t)width * 4, bytes = pitch * (size_t)height;
    unsigned char *pixels = malloc(bytes), *row = malloc(pitch);
    if (!pixels || !row) { free(pixels); free(row); return; }
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    for (int y = 0; y < height / 2; ++y) {
        unsigned char *top = pixels + (size_t)y * pitch;
        unsigned char *bottom = pixels + (size_t)(height - 1 - y) * pitch;
        memcpy(row, top, pitch); memcpy(top, bottom, pitch); memcpy(bottom, row, pitch);
    }
    SDL_Surface *surface = SDL_CreateSurfaceFrom(width, height,
        SDL_PIXELFORMAT_RGBA32, pixels, (int)pitch);
    bongo_path_join(path, sizeof(path), app->data_root, "frame.bmp");
    if (surface) { SDL_SaveBMP(surface, path); SDL_DestroySurface(surface); }
    free(row); free(pixels);
}
