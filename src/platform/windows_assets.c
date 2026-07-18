#include "l2dcat/file.h"
#include "l2dcat/platform.h"
#include "l2dcat/path.h"
#include "l2dcat/sha256.h"

#ifdef _WIN32
#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>

static uint16_t read16(const unsigned char *data) {
    return (uint16_t)(data[0] | (uint16_t)data[1] << 8);
}

static uint32_t read32(const unsigned char *data) {
    uint32_t value = 0;
    for (int i = 3; i >= 0; --i) value = value << 8 | data[i];
    return value;
}

static uint64_t read64(const unsigned char *data) {
    uint64_t value = 0;
    for (int i = 7; i >= 0; --i) value = value << 8 | data[i];
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

static bool extract_pack(const unsigned char *data, size_t size, const char *target,
    L2DCatError *error) {
    const unsigned char magic[8] = {'L', '2', 'D', 'P', 'A', 'K', '1', 0};
    if (size < 12 || memcmp(data, magic, sizeof(magic)) != 0) {
        l2dcat_error_set(error, L2DCAT_ERROR_FORMAT, "Invalid embedded asset pack");
        return false;
    }
    uint32_t count = read32(data + 8);
    size_t offset = 12;
    if (count > 4096) return false;
    for (uint32_t index = 0; index < count; ++index) {
        if (offset + 16 > size) return false;
        uint16_t name_size = read16(data + offset);
        uint32_t file_size = read32(data + offset + 4);
        uint64_t file_offset = read64(data + offset + 8);
        offset += 16;
        char name[L2DCAT_PATH_CAP], path[L2DCAT_PATH_CAP];
        if (!name_size || name_size >= sizeof(name) || offset + name_size > size ||
            file_offset > size || file_size > size - (size_t)file_offset) return false;
        memcpy(name, data + offset, name_size); name[name_size] = '\0';
        offset += name_size;
        if (!safe_name(name) ||
            !l2dcat_path_join(path, sizeof(path), target, name)) {
            l2dcat_error_set(error, L2DCAT_ERROR_FORMAT, "Unsafe embedded asset path");
            return false;
        }
        const char *slash = strrchr(path, '/');
        if (slash) {
            char parent[L2DCAT_PATH_CAP];
            size_t length = (size_t)(slash - path);
            memcpy(parent, path, length); parent[length] = '\0';
            if (!SDL_CreateDirectory(parent)) return false;
        }
        if (!write_file(path, data + (size_t)file_offset, file_size, error)) return false;
    }
    return true;
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
        !extract_pack(data, size, target, error)) return L2DCAT_ERROR_IO;
    return write_file(marker, (const unsigned char *)digest, 64, error)
        ? L2DCAT_OK : L2DCAT_ERROR_IO;
}
#endif
