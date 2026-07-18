#include "l2dcat/file.h"
#include "l2dcat/model.h"
#include "l2dcat/path.h"

#include <stdio.h>
#include <string.h>
#include <yyjson.h>

static bool add_behavior(L2DCatBehaviorCatalog *catalog, const L2DCatModelEntry *model,
    L2DCatBehaviorKind kind, const char *group, int index, const char *label,
    const char *sound) {
    if (catalog->count >= L2DCAT_BEHAVIOR_CAP) return false;
    L2DCatBehaviorEntry *entry = &catalog->entries[catalog->count++];
    entry->kind = kind;
    entry->index = index;
    snprintf(entry->group, sizeof(entry->group), "%s", group ? group : "");
    snprintf(entry->label, sizeof(entry->label), "%s", label ? label : "");
    if (kind == L2DCAT_BEHAVIOR_MOTION)
        snprintf(entry->id, sizeof(entry->id), "%s:motion:%s:%d",
            model->id, group ? group : "", index);
    else snprintf(entry->id, sizeof(entry->id), "%s:expression:%d", model->id, index);
    if (sound && *sound)
        l2dcat_path_join(entry->sound, sizeof(entry->sound), model->directory, sound);
    return true;
}

static bool read_motions(L2DCatBehaviorCatalog *catalog, const L2DCatModelEntry *model,
    yyjson_val *motions) {
    if (!yyjson_is_obj(motions)) return true;
    size_t group_index, group_count;
    yyjson_val *group_key, *items;
    yyjson_obj_foreach(motions, group_index, group_count, group_key, items) {
        if (!yyjson_is_arr(items)) continue;
        const char *group = yyjson_get_str(group_key);
        size_t index, count; yyjson_val *item;
        yyjson_arr_foreach(items, index, count, item) {
            const char *sound = yyjson_get_str(yyjson_obj_get(item, "Sound"));
            char label[L2DCAT_ID_CAP];
            snprintf(label, sizeof(label), "%s %zu", group, index + 1);
            if (!add_behavior(catalog, model, L2DCAT_BEHAVIOR_MOTION, group,
                (int)index, label, sound)) return false;
        }
    }
    return true;
}

static bool read_expressions(L2DCatBehaviorCatalog *catalog,
    const L2DCatModelEntry *model, yyjson_val *expressions) {
    if (!yyjson_is_arr(expressions)) return true;
    size_t index, count; yyjson_val *item;
    yyjson_arr_foreach(expressions, index, count, item) {
        const char *name = yyjson_get_str(yyjson_obj_get(item, "Name"));
        char label[L2DCAT_ID_CAP];
        snprintf(label, sizeof(label), "%s", name ? name : "Expression");
        if (!add_behavior(catalog, model, L2DCAT_BEHAVIOR_EXPRESSION, NULL,
            (int)index, label, NULL)) return false;
    }
    return true;
}

L2DCatResult l2dcat_behaviors_load(L2DCatBehaviorCatalog *catalog,
    const L2DCatModelEntry *model, L2DCatError *error) {
    if (!catalog || !model) return L2DCAT_ERROR_ARGUMENT;
    memset(catalog, 0, sizeof(*catalog));
    char path[L2DCAT_PATH_CAP];
    if (!l2dcat_path_join(path, sizeof(path), model->directory, model->setting_file))
        return L2DCAT_ERROR_FORMAT;
    yyjson_read_err json_error = {0};
    FILE *file = l2dcat_file_open(path, "rb");
    yyjson_doc *document = file ? yyjson_read_fp(file, 0, NULL, &json_error) : NULL;
    if (file) fclose(file);
    if (!document) {
        l2dcat_error_set(error, L2DCAT_ERROR_FORMAT, "Cannot read model setting: %s",
            json_error.msg ? json_error.msg : "cannot open file");
        return L2DCAT_ERROR_FORMAT;
    }
    yyjson_val *references = yyjson_obj_get(yyjson_doc_get_root(document), "FileReferences");
    bool ok = read_motions(catalog, model, yyjson_obj_get(references, "Motions")) &&
        read_expressions(catalog, model, yyjson_obj_get(references, "Expressions"));
    yyjson_doc_free(document);
    if (!ok) {
        l2dcat_error_set(error, L2DCAT_ERROR_FORMAT, "Too many model behaviors");
        return L2DCAT_ERROR_FORMAT;
    }
    return L2DCAT_OK;
}
