#include "l2dcat/file.h"
#include "l2dcat/platform.h"
#include "l2dcat/path.h"
#include "l2dcat/sha256.h"

#ifdef _WIN32
#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>

static bool zero_block(const unsigned char *block) {
    for (size_t i = 0; i < 512; ++i) if (block[i]) return false;
    return true;
}

static size_t octal_size(const unsigned char *text, size_t length) {
    size_t value = 0;
    for (size_t i = 0; i < length; ++i) {
        if (text[i] == 0 || text[i] == ' ') continue;
        if (text[i] < '0' || text[i] > '7') break;
        value = value * 8 + (size_t)(text[i] - '0');
    }
    return value;
}

static bool safe_name(const char *name) {
    if (!name[0] || name[0] == '/' || name[0] == '\\') return false;
    const char *cursor = name;
    while ((cursor = strstr(cursor, ".."))) {
        bool left = cursor == name || cursor[-1] == '/' || cursor[-1] == '\\';
        bool right = !cursor[2] || cursor[2] == '/' || cursor[2] == '\\';
        if (left && right) return false;
        cursor += 2;
    }
    return strchr(name, ':') == NULL;
}

static bool tar_name(const unsigned char *header, char *output, size_t capacity) {
    char name[101], prefix[156];
    memcpy(name, header, 100); name[100] = '\0';
    memcpy(prefix, header + 345, 155); prefix[155] = '\0';
    int length = prefix[0] ? snprintf(output, capacity, "%s/%s", prefix, name) :
        snprintf(output, capacity, "%s", name);
    while (output[0] == '.' && output[1] == '/') memmove(output, output + 2,
        strlen(output + 2) + 1);
    return length > 0 && (size_t)length < capacity && safe_name(output);
}

static bool write_file(const char *path, const unsigned char *data, size_t size,
    L2DCatError *error) {
    FILE *file = l2dcat_file_open(path, "wb");
    if (!file) {
        l2dcat_error_set(error, L2DCAT_ERROR_IO, "Cannot create embedded asset: %s", path);
        return false;
    }
    bool ok = fwrite(data, 1, size, file) == size;
    if (fclose(file) != 0) ok = false;
    if (!ok) l2dcat_error_set(error, L2DCAT_ERROR_IO, "Cannot write embedded asset: %s", path);
    return ok;
}

static bool current_pack(const char *marker, const char *models,
    const char expected[65]) {
    if (!l2dcat_path_is_dir(models)) return false;
    FILE *file = l2dcat_file_open(marker, "rb");
    if (!file) return false;
    char actual[65] = {0};
    bool read = fread(actual, 1, 64, file) == 64;
    fclose(file);
    return read && strcmp(actual, expected) == 0;
}

static bool extract_tar(const unsigned char *data, size_t size, const char *target,
    L2DCatError *error) {
    size_t offset = 0;
    while (offset + 512 <= size) {
        const unsigned char *header = data + offset;
        if (zero_block(header)) return true;
        char name[L2DCAT_PATH_CAP], path[L2DCAT_PATH_CAP];
        if (!tar_name(header, name, sizeof(name)) ||
            !l2dcat_path_join(path, sizeof(path), target, name)) {
            l2dcat_error_set(error, L2DCAT_ERROR_FORMAT, "Unsafe embedded asset path");
            return false;
        }
        size_t file_size = octal_size(header + 124, 12);
        size_t blocks = (file_size + 511) / 512;
        if (offset + 512 + blocks * 512 > size) {
            l2dcat_error_set(error, L2DCAT_ERROR_FORMAT, "Truncated embedded asset pack");
            return false;
        }
        char type = (char)header[156];
        if (type == '5') {
            if (!SDL_CreateDirectory(path)) return false;
        } else if (type == 0 || type == '0') {
            const char *slash = strrchr(path, '/');
            if (slash) {
                char parent[L2DCAT_PATH_CAP];
                size_t length = (size_t)(slash - path);
                memcpy(parent, path, length); parent[length] = '\0';
                if (!SDL_CreateDirectory(parent)) return false;
            }
            if (!write_file(path, header + 512, file_size, error)) return false;
        }
        offset += 512 + blocks * 512;
    }
    l2dcat_error_set(error, L2DCAT_ERROR_FORMAT, "Invalid embedded asset pack");
    return false;
}

L2DCatResult l2dcat_platform_embedded_assets(const char *target, L2DCatError *error) {
    if (!target) return L2DCAT_ERROR_ARGUMENT;
    char marker[L2DCAT_PATH_CAP], models[L2DCAT_PATH_CAP];
    l2dcat_path_join(marker, sizeof(marker), target, "complete");
    l2dcat_path_join(models, sizeof(models), target, "assets/models");
    HRSRC resource = FindResourceW(NULL, MAKEINTRESOURCEW(101), MAKEINTRESOURCEW(10));
    if (!resource) return L2DCAT_ERROR_IO;
    HGLOBAL loaded = LoadResource(NULL, resource);
    const unsigned char *data = loaded ? LockResource(loaded) : NULL;
    DWORD size = loaded ? SizeofResource(NULL, resource) : 0;
    char digest[65];
    if (data && size) l2dcat_sha256_bytes(data, size, digest);
    if (data && size && current_pack(marker, models, digest)) return L2DCAT_OK;
    if (!data || !size || !SDL_CreateDirectory(target) ||
        !extract_tar(data, size, target, error)) return L2DCAT_ERROR_IO;
    return write_file(marker, (const unsigned char *)digest, 64, error)
        ? L2DCAT_OK : L2DCAT_ERROR_IO;
}
#endif
