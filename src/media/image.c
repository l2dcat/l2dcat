#include "l2dcat/file.h"
#include "l2dcat/image.h"

#ifdef _WIN32
#define COBJMACROS
#include <objbase.h>
#include <wincodec.h>
#include <windows.h>
#endif
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <stb_image.h>
#include <stdlib.h>
#include <string.h>

#define L2DCAT_LIVE2D_TEXTURE_LIMIT 4096

#ifdef _WIN32
static wchar_t *wide_path(const char *path) {
    int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
        path, -1, NULL, 0);
    wchar_t *wide = count > 0 ? malloc((size_t)count * sizeof(*wide)) : NULL;
    if (wide && !MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
        path, -1, wide, count)) {
        free(wide); return NULL;
    }
    return wide;
}

static bool wic_texture_image(const char *path, L2DCatImage *image) {
    memset(image, 0, sizeof(*image));
    HRESULT initialized = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    bool uninitialize = initialized == S_OK || initialized == S_FALSE;
    if (FAILED(initialized) && initialized != RPC_E_CHANGED_MODE) return false;
    IWICImagingFactory *factory = NULL;
    IWICBitmapDecoder *decoder = NULL;
    IWICBitmapFrameDecode *frame = NULL;
    IWICBitmapScaler *scaler = NULL;
    IWICFormatConverter *converter = NULL;
    wchar_t *wide = wide_path(path);
    HRESULT result = wide ? CoCreateInstance(&CLSID_WICImagingFactory, NULL,
        CLSCTX_INPROC_SERVER, &IID_IWICImagingFactory, (void **)&factory) : E_FAIL;
    if (SUCCEEDED(result)) result = IWICImagingFactory_CreateDecoderFromFilename(factory,
        wide, NULL, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder);
    if (SUCCEEDED(result)) result = IWICBitmapDecoder_GetFrame(decoder, 0, &frame);
    UINT source_width = 0, source_height = 0;
    if (SUCCEEDED(result)) result = IWICBitmapFrameDecode_GetSize(frame,
        &source_width, &source_height);
    UINT target_width = source_width, target_height = source_height;
    if (source_width > L2DCAT_LIVE2D_TEXTURE_LIMIT ||
        source_height > L2DCAT_LIVE2D_TEXTURE_LIMIT) {
        if (source_width >= source_height) {
            target_width = L2DCAT_LIVE2D_TEXTURE_LIMIT;
            target_height = (UINT)((uint64_t)source_height * target_width / source_width);
        } else {
            target_height = L2DCAT_LIVE2D_TEXTURE_LIMIT;
            target_width = (UINT)((uint64_t)source_width * target_height / source_height);
        }
        if (!target_width) target_width = 1;
        if (!target_height) target_height = 1;
        if (SUCCEEDED(result)) result = IWICImagingFactory_CreateBitmapScaler(factory, &scaler);
        if (SUCCEEDED(result)) result = IWICBitmapScaler_Initialize(scaler,
            (IWICBitmapSource *)frame, target_width, target_height,
            WICBitmapInterpolationModeFant);
    }
    IWICBitmapSource *source = scaler ? (IWICBitmapSource *)scaler :
        (IWICBitmapSource *)frame;
    if (SUCCEEDED(result))
        result = IWICImagingFactory_CreateFormatConverter(factory, &converter);
    if (SUCCEEDED(result)) result = IWICFormatConverter_Initialize(converter, source,
        &GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, NULL, 0,
        WICBitmapPaletteTypeCustom);
    size_t stride = (size_t)target_width * 4;
    size_t bytes = stride * target_height;
    unsigned char *pixels = bytes <= UINT_MAX ? malloc(bytes) : NULL;
    if (SUCCEEDED(result) && pixels) result = IWICFormatConverter_CopyPixels(converter,
        NULL, (UINT)stride, (UINT)bytes, pixels);
    bool ok = SUCCEEDED(result) && pixels;
    if (ok) {
        image->pixels = pixels;
        image->width = (int)target_width;
        image->height = (int)target_height;
    } else free(pixels);
    if (converter) IWICFormatConverter_Release(converter);
    if (scaler) IWICBitmapScaler_Release(scaler);
    if (frame) IWICBitmapFrameDecode_Release(frame);
    if (decoder) IWICBitmapDecoder_Release(decoder);
    if (factory) IWICImagingFactory_Release(factory);
    free(wide);
    if (uninitialize) CoUninitialize();
    return ok;
}
#endif

L2DCatResult l2dcat_image_load(const char *path, L2DCatImage *image, L2DCatError *error) {
    if (!path || !image) return L2DCAT_ERROR_ARGUMENT;
    memset(image, 0, sizeof(*image));
    int channels;
    FILE *file = l2dcat_file_open(path, "rb");
    image->pixels = file ? stbi_load_from_file(file, &image->width,
        &image->height, &channels, STBI_rgb_alpha) : NULL;
    image->pixels_stbi = image->pixels != NULL;
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
    if (image->pixels) {
        if (image->pixels_stbi) stbi_image_free(image->pixels);
        else free(image->pixels);
    }
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
#ifdef _WIN32
    if (!wic_texture_image(path, &image) &&
        l2dcat_image_load(path, &image, error) != L2DCAT_OK) return 0;
#else
    if (l2dcat_image_load(path, &image, error) != L2DCAT_OK) return 0;
#endif
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
