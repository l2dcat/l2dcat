#include "l2dcat/file.h"
#include "l2dcat/image.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <stb_image.h>
#include <string.h>

L2DCatResult l2dcat_image_load(const char *path, L2DCatImage *image, L2DCatError *error) {
    if (!path || !image) return L2DCAT_ERROR_ARGUMENT;
    memset(image, 0, sizeof(*image));
    int channels;
    FILE *file = l2dcat_file_open(path, "rb");
    image->pixels = file ? stbi_load_from_file(file, &image->width,
        &image->height, &channels, STBI_rgb_alpha) : NULL;
    if (file) fclose(file);
    if (!image->pixels) {
        l2dcat_error_set(error, L2DCAT_ERROR_IO, "Cannot decode PNG: %s", path);
        return L2DCAT_ERROR_IO;
    }
    image->surface = SDL_CreateSurfaceFrom(image->width, image->height,
        SDL_PIXELFORMAT_RGBA32, image->pixels, image->width * 4);
    if (!image->surface) {
        l2dcat_image_free(image);
        l2dcat_error_set(error, L2DCAT_ERROR_PLATFORM, "Cannot create image surface");
        return L2DCAT_ERROR_PLATFORM;
    }
    return L2DCAT_OK;
}

void l2dcat_image_free(L2DCatImage *image) {
    if (!image) return;
    if (image->surface) SDL_DestroySurface(image->surface);
    if (image->pixels) stbi_image_free(image->pixels);
    memset(image, 0, sizeof(*image));
}

static unsigned int upload(const L2DCatImage *image, GLuint texture, bool mipmapped) {
    bool created = texture == 0;
    if (created) glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    if (created) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
            mipmapped ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    if (created) glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, image->width,
        image->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image->pixels);
    else glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, image->width, image->height,
        GL_RGBA, GL_UNSIGNED_BYTE, image->pixels);
    if (created && mipmapped) {
        PFNGLGENERATEMIPMAPPROC generate_mipmap =
            (PFNGLGENERATEMIPMAPPROC)SDL_GL_GetProcAddress("glGenerateMipmap");
        if (generate_mipmap) generate_mipmap(GL_TEXTURE_2D);
    }
    return texture;
}

unsigned int l2dcat_image_texture(const char *path, int *width, int *height, L2DCatError *error) {
    L2DCatImage image;
    if (l2dcat_image_load(path, &image, error) != L2DCAT_OK) return 0;
    GLuint texture = upload(&image, 0, false);
    if (width) *width = image.width;
    if (height) *height = image.height;
    l2dcat_image_free(&image);
    return texture;
}

unsigned int l2dcat_image_texture_mipmapped(const char *path, int *width, int *height,
    L2DCatError *error) {
    L2DCatImage image;
    if (l2dcat_image_load(path, &image, error) != L2DCAT_OK) return 0;
    GLuint texture = upload(&image, 0, true);
    if (width) *width = image.width;
    if (height) *height = image.height;
    l2dcat_image_free(&image);
    return texture;
}

static void erase_paw(L2DCatImage *image, bool left) {
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

static bool blend_file(L2DCatImage *base, const char *path, L2DCatError *error) {
    if (!path || !path[0]) return true;
    L2DCatImage layer;
    if (l2dcat_image_load(path, &layer, error) != L2DCAT_OK) return false;
    bool valid = layer.width == base->width && layer.height == base->height;
    if (valid) for (int i = 0; i < base->width * base->height; ++i) {
        unsigned char *dst = base->pixels + i * 4, *src = layer.pixels + i * 4;
        unsigned alpha = src[3], inverse = 255 - alpha;
        for (int channel = 0; channel < 3; ++channel)
            dst[channel] = (unsigned char)((src[channel] * alpha +
                dst[channel] * inverse + 127) / 255);
        dst[3] = (unsigned char)(alpha + (dst[3] * inverse + 127) / 255);
    }
    l2dcat_image_free(&layer);
    return valid;
}

unsigned int l2dcat_image_composite_texture(const char *base, const char *left,
    const char *right, unsigned int texture, bool erase_left, bool erase_right,
    L2DCatError *error) {
    L2DCatImage image;
    if (l2dcat_image_load(base, &image, error) != L2DCAT_OK) return 0;
    if (erase_left) erase_paw(&image, true);
    if (erase_right) erase_paw(&image, false);
    bool valid = blend_file(&image, left, error) && blend_file(&image, right, error);
    if (valid) texture = upload(&image, texture, false);
    l2dcat_image_free(&image);
    return texture;
}
