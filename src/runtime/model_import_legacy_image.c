#include "model_import_legacy_internal.h"
#include "l2dcat/file.h"
#include "l2dcat/image.h"
#include "l2dcat/path.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stb_image_write.h>

typedef struct PngWriter { FILE *file; bool ok; } PngWriter;

static void write_png(void *context, void *data, int size) {
    PngWriter *writer = context;
    if (writer->ok && fwrite(data, 1, (size_t)size, writer->file) != (size_t)size)
        writer->ok = false;
}

static bool compose(const char *base_path, const char *hand_path,
    const char *target, L2DCatError *error) {
    L2DCatImage base, hand;
    if (l2dcat_image_load(base_path, &base, error) != L2DCAT_OK) return false;
    if (l2dcat_image_load(hand_path, &hand, error) != L2DCAT_OK) {
        l2dcat_image_free(&base); return false;
    }
    int width = SDL_min(base.width, hand.width), height = SDL_min(base.height, hand.height);
    size_t bytes = (size_t)width * (size_t)height * 4;
    unsigned char *pixels = width > 0 && height > 0 ? malloc(bytes) : NULL;
    bool allocated = pixels != NULL;
    if (pixels) for (int y = 0; y < height; ++y)
        memcpy(pixels + (size_t)y * width * 4,
            base.pixels + (size_t)y * base.width * 4, (size_t)width * 4);
    if (pixels) for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x) {
        unsigned char *dst = pixels + ((size_t)y * width + x) * 4;
        const unsigned char *src = hand.pixels + ((size_t)y * hand.width + x) * 4;
        unsigned alpha = src[3], inverse = 255 - alpha, destination_alpha = dst[3];
        unsigned output_alpha = alpha * 255 + destination_alpha * inverse;
        if (!output_alpha) { memset(dst, 0, 4); continue; }
        for (int channel = 0; channel < 3; ++channel) {
            unsigned color = src[channel] * alpha * 255 +
                dst[channel] * destination_alpha * inverse;
            dst[channel] = (unsigned char)((color + output_alpha / 2) / output_alpha);
        }
        dst[3] = (unsigned char)((output_alpha + 127) / 255);
    }
    FILE *file = pixels ? l2dcat_file_open(target, "wb") : NULL;
    PngWriter writer = {file, file != NULL};
    bool ok = file && stbi_write_png_to_func(write_png, &writer, width, height, 4,
        pixels, width * 4) && writer.ok;
    if (file && fclose(file) != 0) ok = false;
    free(pixels); l2dcat_image_free(&hand); l2dcat_image_free(&base);
    if (!ok) l2dcat_error_set(error, allocated ? L2DCAT_ERROR_IO : L2DCAT_ERROR_MEMORY,
        "Cannot compose legacy input image: %s", target);
    return ok;
}

bool l2dcat_legacy_emit_pair(const char *hand, const char *keyboard,
    const char *directory, L2DCatLegacyKeyNames names, L2DCatError *error) {
    if (!names.count) return true;
    char first_name[32], first[L2DCAT_PATH_CAP];
    const char *first_item = names.items[0] ? names.items[0] : names.generated;
    snprintf(first_name, sizeof(first_name), "%s.png", first_item);
    if (!l2dcat_path_join(first, sizeof(first), directory, first_name)) return false;
    bool ok;
    if (keyboard) ok = compose(keyboard, hand, first, error);
    else {
        L2DCatImage image;
        ok = l2dcat_image_load(hand, &image, error) == L2DCAT_OK;
        if (ok) { l2dcat_image_free(&image); ok = SDL_CopyFile(hand, first); }
    }
    if (!ok && error && !error->message[0])
        l2dcat_error_set(error, L2DCAT_ERROR_IO, "Cannot copy legacy input image: %s", hand);
    for (size_t i = 1; ok && i < names.count; ++i) {
        char filename[32], target[L2DCAT_PATH_CAP];
        const char *item = names.items[i] ? names.items[i] : names.generated;
        snprintf(filename, sizeof(filename), "%s.png", item);
        ok = l2dcat_path_join(target, sizeof(target), directory, filename) &&
            SDL_CopyFile(first, target);
    }
    return ok;
}
