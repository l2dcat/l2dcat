#include "bongo_cat_neo/file.h"
#include "bongo_cat_neo/platform.h"
#include "bongo_cat_neo/path.h"
#include "bongo_cat_neo/sha256.h"

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
    BongoCatNeoError *error) {
    FILE *file = bongo_cat_neo_file_open(path, "wb");
    if (!file) {
        bongo_cat_neo_error_set(error, BONGO_CAT_NEO_ERROR_IO, "Cannot create embedded asset: %s", path);
        return false;
    }
    bool ok = fwrite(data, 1, size, file) == size;
    if (fclose(file) != 0) ok = false;
    if (!ok) bongo_cat_neo_error_set(error, BONGO_CAT_NEO_ERROR_IO, "Cannot write embedded asset: %s", path);
    return ok;
}

static bool current_pack(const char *marker, const char *models,
    const char expected[65]) {
    if (!bongo_cat_neo_path_is_dir(models)) return false;
    FILE *file = bongo_cat_neo_file_open(marker, "rb");
    if (!file) return false;
    char actual[65] = {0};
    bool read = fread(actual, 1, 64, file) == 64;
    fclose(file);
    return read && strcmp(actual, expected) == 0;
}

static bool extract_pack(const unsigned char *data, size_t size, const char *target,
    BongoCatNeoError *error) {
    const unsigned char magic[8] = {'L', '2', 'D', 'P', 'A', 'K', '1', 0};
    if (size < 12 || memcmp(data, magic, sizeof(magic)) != 0) {
        bongo_cat_neo_error_set(error, BONGO_CAT_NEO_ERROR_FORMAT, "Invalid embedded asset pack");
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
        char name[BONGO_CAT_NEO_PATH_CAP], path[BONGO_CAT_NEO_PATH_CAP];
        if (!name_size || name_size >= sizeof(name) || offset + name_size > size ||
            file_offset > size || file_size > size - (size_t)file_offset) return false;
        memcpy(name, data + offset, name_size); name[name_size] = '\0';
        offset += name_size;
        if (!safe_name(name) ||
            !bongo_cat_neo_path_join(path, sizeof(path), target, name)) {
            bongo_cat_neo_error_set(error, BONGO_CAT_NEO_ERROR_FORMAT, "Unsafe embedded asset path");
            return false;
        }
        const char *slash = strrchr(path, '/');
        if (slash) {
            char parent[BONGO_CAT_NEO_PATH_CAP];
            size_t length = (size_t)(slash - path);
            memcpy(parent, path, length); parent[length] = '\0';
            if (!SDL_CreateDirectory(parent)) return false;
        }
        if (!write_file(path, data + (size_t)file_offset, file_size, error)) return false;
    }
    return true;
}

BongoCatNeoResult bongo_cat_neo_platform_embedded_assets(const char *target, BongoCatNeoError *error) {
    if (!target) return BONGO_CAT_NEO_ERROR_ARGUMENT;
    char marker[BONGO_CAT_NEO_PATH_CAP], models[BONGO_CAT_NEO_PATH_CAP];
    bongo_cat_neo_path_join(marker, sizeof(marker), target, "complete");
    bongo_cat_neo_path_join(models, sizeof(models), target, "assets/models");
    HRSRC resource = FindResourceW(NULL, MAKEINTRESOURCEW(101), MAKEINTRESOURCEW(10));
    if (!resource) return BONGO_CAT_NEO_ERROR_IO;
    HGLOBAL loaded = LoadResource(NULL, resource);
    const unsigned char *data = loaded ? LockResource(loaded) : NULL;
    DWORD size = loaded ? SizeofResource(NULL, resource) : 0;
    char digest[65];
    if (data && size) bongo_cat_neo_sha256_bytes(data, size, digest);
    if (data && size && current_pack(marker, models, digest)) return BONGO_CAT_NEO_OK;
    if (!data || !size || !SDL_CreateDirectory(target) ||
        !extract_pack(data, size, target, error)) return BONGO_CAT_NEO_ERROR_IO;
    return write_file(marker, (const unsigned char *)digest, 64, error)
        ? BONGO_CAT_NEO_OK : BONGO_CAT_NEO_ERROR_IO;
}
#endif
