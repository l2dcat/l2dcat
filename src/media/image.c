#include "bongo/file.h"
#include "bongo/image.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <stb_image.h>
#include <string.h>

BongoResult bongo_image_load(const char *path, BongoImage *image, BongoError *error) {
    if (!path || !image) return BONGO_ERROR_ARGUMENT;
    memset(image, 0, sizeof(*image));
    int channels;
    FILE *file = bongo_file_open(path, "rb");
    image->pixels = file ? stbi_load_from_file(file, &image->width,
        &image->height, &channels, STBI_rgb_alpha) : NULL;
    if (file) fclose(file);
    if (!image->pixels) {
        bongo_error_set(error, BONGO_ERROR_IO, "Cannot decode PNG: %s", path);
        return BONGO_ERROR_IO;
    }
    image->surface = SDL_CreateSurfaceFrom(image->width, image->height,
        SDL_PIXELFORMAT_RGBA32, image->pixels, image->width * 4);
    if (!image->surface) {
        bongo_image_free(image);
        bongo_error_set(error, BONGO_ERROR_PLATFORM, "Cannot create image surface");
        return BONGO_ERROR_PLATFORM;
    }
    return BONGO_OK;
}

void bongo_image_free(BongoImage *image) {
    if (!image) return;
    if (image->surface) SDL_DestroySurface(image->surface);
    if (image->pixels) stbi_image_free(image->pixels);
    memset(image, 0, sizeof(*image));
}

static unsigned int upload(const BongoImage *image, GLuint texture) {
    bool created = texture == 0;
    if (created) glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    if (created) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    if (created) glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, image->width,
        image->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image->pixels);
    else glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, image->width, image->height,
        GL_RGBA, GL_UNSIGNED_BYTE, image->pixels);
    return texture;
}

unsigned int bongo_image_texture(const char *path, int *width, int *height, BongoError *error) {
    BongoImage image;
    if (bongo_image_load(path, &image, error) != BONGO_OK) return 0;
    GLuint texture = upload(&image, 0);
    if (width) *width = image.width;
    if (height) *height = image.height;
    bongo_image_free(&image);
    return texture;
}

static void erase_paw(BongoImage *image, bool left) {
    float cx = left ? .700f : .275f, cy = left ? .515f : .397f;
    float rx = left ? .080f : .070f, ry = left ? .170f : .160f;
    for (int y = 0; y < image->height; ++y) for (int x = 0; x < image->width; ++x) {
        float dx = (x / (float)image->width - cx) / rx;
        float dy = (y / (float)image->height - cy) / ry;
        unsigned char *pixel = image->pixels + ((size_t)y * image->width + x) * 4;
        if (dx * dx + dy * dy < 1.0f && pixel[3])
            pixel[0] = pixel[1] = pixel[2] = 255;
    }
}

static bool blend_file(BongoImage *base, const char *path, BongoError *error) {
    if (!path || !path[0]) return true;
    BongoImage layer;
    if (bongo_image_load(path, &layer, error) != BONGO_OK) return false;
    bool valid = layer.width == base->width && layer.height == base->height;
    if (valid) for (int i = 0; i < base->width * base->height; ++i) {
        unsigned char *dst = base->pixels + i * 4, *src = layer.pixels + i * 4;
        unsigned alpha = src[3], inverse = 255 - alpha;
        for (int channel = 0; channel < 3; ++channel)
            dst[channel] = (unsigned char)((src[channel] * alpha +
                dst[channel] * inverse + 127) / 255);
        dst[3] = (unsigned char)(alpha + (dst[3] * inverse + 127) / 255);
    }
    bongo_image_free(&layer);
    return valid;
}

unsigned int bongo_image_composite_texture(const char *base, const char *left,
    const char *right, unsigned int texture, bool erase_left, bool erase_right,
    BongoError *error) {
    BongoImage image;
    if (bongo_image_load(base, &image, error) != BONGO_OK) return 0;
    if (erase_left) erase_paw(&image, true);
    if (erase_right) erase_paw(&image, false);
    bool valid = blend_file(&image, left, error) && blend_file(&image, right, error);
    if (valid) texture = upload(&image, texture);
    bongo_image_free(&image);
    return texture;
}
