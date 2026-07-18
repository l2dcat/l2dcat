#ifndef L2DCAT_I18N_H
#define L2DCAT_I18N_H

#include "l2dcat/config.h"

typedef struct L2DCatI18n L2DCatI18n;

L2DCatI18n *l2dcat_i18n_create(const char *root, L2DCatLanguage language, L2DCatError *error);
void l2dcat_i18n_destroy(L2DCatI18n *i18n);
L2DCatResult l2dcat_i18n_reload(L2DCatI18n *i18n, L2DCatLanguage language,
    L2DCatError *error);
const char *l2dcat_i18n_get(const L2DCatI18n *i18n, const char *key,
    const char *fallback);
size_t l2dcat_i18n_glyph_ranges(const L2DCatI18n *i18n, uint32_t *ranges,
    size_t capacity);

#endif
