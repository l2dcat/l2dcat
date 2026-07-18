#include "l2dcat/i18n.h"

#include <stdio.h>
#include <string.h>
#include <yyjson.h>

static yyjson_doc *load(const char *root, const char *name) {
    char path[L2DCAT_PATH_CAP];
    snprintf(path, sizeof(path), "%s/%s.json", root, name);
    return yyjson_read_file(path, 0, NULL, NULL);
}

static bool same_shape(yyjson_val *reference, yyjson_val *candidate,
    const char *path) {
    if (!reference || !candidate || yyjson_get_type(reference) != yyjson_get_type(candidate)) {
        fprintf(stderr, "Locale mismatch at %s\n", path);
        return false;
    }
    if (!yyjson_is_obj(reference)) return true;
    size_t index, count; yyjson_val *key, *value;
    yyjson_obj_foreach(reference, index, count, key, value) {
        const char *name = yyjson_get_str(key);
        yyjson_val *next = yyjson_obj_get(candidate, name);
        char child[L2DCAT_PATH_CAP];
        snprintf(child, sizeof(child), "%s%s%s", path, path[0] ? "." : "", name);
        if (!same_shape(value, next, child)) return false;
    }
    return true;
}

static bool includes(const uint32_t *ranges, uint32_t point) {
    for (size_t i = 0; ranges[i] && ranges[i + 1]; i += 2)
        if (point >= ranges[i] && point <= ranges[i + 1]) return true;
    return false;
}

int main(void) {
    char root[L2DCAT_PATH_CAP];
    snprintf(root, sizeof(root), "%s/resources/assets/locales", L2DCAT_NATIVE_SOURCE_DIR);
    yyjson_doc *reference = load(root, "en-US");
    if (!reference) return 1;
    const uint32_t expected[] = {'A', 0x4e2d, 0x8a2d, 0x00ea, 0x1ebf};
    for (int language = 0; language <= L2DCAT_LANG_VI_VN; ++language) {
        const char *name = l2dcat_language_name((L2DCatLanguage)language);
        yyjson_doc *document = load(root, name);
        if (!document || !same_shape(yyjson_doc_get_root(reference),
            yyjson_doc_get_root(document), "")) return 2;
        yyjson_doc_free(document);
        L2DCatError error = {0};
        L2DCatI18n *i18n = l2dcat_i18n_create(root, (L2DCatLanguage)language, &error);
        uint32_t ranges[2048];
        if (!i18n || l2dcat_i18n_glyph_ranges(i18n, ranges, 2048) < 3 ||
            ranges[0] != 0x20 || !includes(ranges, expected[language])) {
            fprintf(stderr, "Missing U+%04X for %s\n", expected[language], name);
            return 3;
        }
        l2dcat_i18n_destroy(i18n);
    }
    yyjson_doc_free(reference);
    puts("i18n smoke passed");
    return 0;
}
