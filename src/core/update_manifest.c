#include "l2dcat/file.h"
#include "l2dcat/update_manifest.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <yyjson.h>

typedef struct ParsedVersion {
    uint32_t core[3];
    const char *prerelease;
    const char *prerelease_end;
} ParsedVersion;

static bool identifier_char(char value) {
    return isalnum((unsigned char)value) || value == '-';
}

static bool parse_identifiers(const char **cursor, bool prerelease,
    const char **end) {
    const char *value = *cursor;
    for (;;) {
        const char *start = value;
        bool numeric = true;
        while (identifier_char(*value)) {
            if (!isdigit((unsigned char)*value)) numeric = false;
            ++value;
        }
        if (value == start || (prerelease && numeric && value - start > 1 && *start == '0'))
            return false;
        if (*value != '.') break;
        ++value;
    }
    *cursor = value;
    if (end) *end = value;
    return true;
}

static bool parse_version(const char *text, ParsedVersion *parsed) {
    if (!text || !*text || !parsed) return false;
    memset(parsed, 0, sizeof(*parsed));
    const char *cursor = (*text == 'v' || *text == 'V') ? text + 1 : text;
    for (size_t index = 0; index < 3; ++index) {
        const char *start = cursor;
        uint64_t value = 0;
        while (isdigit((unsigned char)*cursor)) {
            value = value * 10 + (uint64_t)(*cursor - '0');
            if (value > UINT32_MAX) return false;
            ++cursor;
        }
        if (cursor == start || (cursor - start > 1 && *start == '0')) return false;
        parsed->core[index] = (uint32_t)value;
        if (*cursor != '.') break;
        if (index == 2) return false;
        ++cursor;
    }
    if (*cursor == '-') {
        parsed->prerelease = ++cursor;
        if (!parse_identifiers(&cursor, true, &parsed->prerelease_end)) return false;
    }
    if (*cursor == '+') {
        ++cursor;
        if (!parse_identifiers(&cursor, false, NULL)) return false;
    }
    return *cursor == '\0';
}

static int compare_numeric_identifier(const char *left, size_t left_length,
    const char *right, size_t right_length) {
    if (left_length != right_length) return left_length > right_length ? 1 : -1;
    int result = memcmp(left, right, left_length);
    return result > 0 ? 1 : result < 0 ? -1 : 0;
}

static int compare_prerelease(const ParsedVersion *left, const ParsedVersion *right) {
    if (!left->prerelease || !right->prerelease)
        return left->prerelease ? -1 : right->prerelease ? 1 : 0;
    const char *a = left->prerelease, *b = right->prerelease;
    while (a < left->prerelease_end && b < right->prerelease_end) {
        const char *ae = a, *be = b;
        while (ae < left->prerelease_end && *ae != '.') ++ae;
        while (be < right->prerelease_end && *be != '.') ++be;
        bool an = true, bn = true;
        for (const char *value = a; value < ae; ++value)
            if (!isdigit((unsigned char)*value)) an = false;
        for (const char *value = b; value < be; ++value)
            if (!isdigit((unsigned char)*value)) bn = false;
        int result;
        if (an != bn) result = an ? -1 : 1;
        else if (an) result = compare_numeric_identifier(a, (size_t)(ae - a),
            b, (size_t)(be - b));
        else {
            size_t length = (size_t)(ae - a) < (size_t)(be - b) ?
                (size_t)(ae - a) : (size_t)(be - b);
            result = memcmp(a, b, length);
            if (!result && ae - a != be - b) result = ae - a > be - b ? 1 : -1;
            result = result > 0 ? 1 : result < 0 ? -1 : 0;
        }
        if (result) return result;
        a = ae < left->prerelease_end ? ae + 1 : ae;
        b = be < right->prerelease_end ? be + 1 : be;
    }
    return a < left->prerelease_end ? 1 : b < right->prerelease_end ? -1 : 0;
}

int l2dcat_version_compare(const char *left, const char *right) {
    ParsedVersion a, b;
    if (!parse_version(left, &a) || !parse_version(right, &b)) return 0;
    for (size_t index = 0; index < 3; ++index) {
        if (a.core[index] != b.core[index])
            return a.core[index] > b.core[index] ? 1 : -1;
    }
    return compare_prerelease(&a, &b);
}

static bool copy_required(char *target, size_t capacity, yyjson_val *value) {
    if (!yyjson_is_str(value)) return false;
    const char *text = yyjson_get_str(value);
    size_t length = yyjson_get_len(value);
    if (!text || length == 0 || length >= capacity) return false;
    memcpy(target, text, length + 1); return true;
}

static void copy_optional(char *target, size_t capacity, yyjson_val *value) {
    if (!yyjson_is_str(value) || !capacity) return;
    snprintf(target, capacity, "%s", yyjson_get_str(value));
}

static bool valid_hash(const char *value) {
    if (!value || strlen(value) != 64) return false;
    for (size_t i = 0; i < 64; ++i) if (!isxdigit((unsigned char)value[i])) return false;
    return true;
}

static bool valid_https_url(const char *value) {
    if (!value || strncmp(value, "https://", 8) != 0) return false;
    const char *host = value + 8;
    if (!*host || *host == '/' || *host == '?' || *host == '#') return false;
    bool authority = true;
    for (const unsigned char *cursor = (const unsigned char *)host; *cursor; ++cursor) {
        if (*cursor <= 0x20 || *cursor == 0x7f || *cursor == '\\' || *cursor == '#')
            return false;
        if (authority && *cursor == '@') return false;
        if (*cursor == '/' || *cursor == '?') authority = false;
    }
    return true;
}

L2DCatResult l2dcat_update_manifest_load(const char *path, const char *platform,
    L2DCatUpdateManifest *manifest, L2DCatError *error) {
    if (!path || !platform || !manifest) return L2DCAT_ERROR_ARGUMENT;
    yyjson_read_err json_error = {0};
    FILE *file = l2dcat_file_open(path, "rb");
    yyjson_doc *doc = file ? yyjson_read_fp(file, 0, NULL, &json_error) : NULL;
    if (file) fclose(file);
    if (!doc) {
        l2dcat_error_set(error, L2DCAT_ERROR_FORMAT, "Invalid update manifest: %s",
            json_error.msg ? json_error.msg : "cannot open file");
        return L2DCAT_ERROR_FORMAT;
    }
    memset(manifest, 0, sizeof(*manifest));
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *platforms = yyjson_obj_get(root, "platforms");
    yyjson_val *entry = yyjson_is_obj(platforms) ? yyjson_obj_get(platforms, platform) : NULL;
    bool fields = yyjson_is_obj(root) && yyjson_is_obj(entry) &&
        copy_required(manifest->version, sizeof(manifest->version), yyjson_obj_get(root, "version")) &&
        copy_required(manifest->url, sizeof(manifest->url), yyjson_obj_get(entry, "url")) &&
        copy_required(manifest->sha256, sizeof(manifest->sha256), yyjson_obj_get(entry, "sha256"));
    if (fields) {
        copy_optional(manifest->notes, sizeof(manifest->notes), yyjson_obj_get(root, "notes"));
        copy_optional(manifest->published, sizeof(manifest->published), yyjson_obj_get(root, "pub_date"));
        copy_optional(manifest->signature, sizeof(manifest->signature),
            yyjson_obj_get(entry, "signature"));
        yyjson_val *size = yyjson_obj_get(entry, "size");
        if (yyjson_is_uint(size)) manifest->size = yyjson_get_uint(size);
    }
    yyjson_doc_free(doc);
    ParsedVersion version;
    if (!fields || !parse_version(manifest->version, &version) ||
        !valid_https_url(manifest->url) ||
        !valid_hash(manifest->sha256)) {
        l2dcat_error_set(error, L2DCAT_ERROR_FORMAT,
            "Update manifest requires version, HTTPS URL, and SHA-256");
        return L2DCAT_ERROR_FORMAT;
    }
    for (size_t i = 0; i < 64; ++i) manifest->sha256[i] =
        (char)tolower((unsigned char)manifest->sha256[i]);
    return L2DCAT_OK;
}
