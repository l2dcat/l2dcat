#ifndef BONGO_CAT_NEO_I18N_H
#define BONGO_CAT_NEO_I18N_H

#include "bongo_cat_neo/config.h"

typedef struct BongoCatNeoI18n BongoCatNeoI18n;

BongoCatNeoI18n *bongo_cat_neo_i18n_create(const char *root, BongoCatNeoLanguage language, BongoCatNeoError *error);
void bongo_cat_neo_i18n_destroy(BongoCatNeoI18n *i18n);
BongoCatNeoResult bongo_cat_neo_i18n_reload(BongoCatNeoI18n *i18n, BongoCatNeoLanguage language,
    BongoCatNeoError *error);
const char *bongo_cat_neo_i18n_get(const BongoCatNeoI18n *i18n, const char *key,
    const char *fallback);
size_t bongo_cat_neo_i18n_glyph_ranges(const BongoCatNeoI18n *i18n, uint32_t *ranges,
    size_t capacity);
size_t bongo_cat_neo_i18n_all_glyph_ranges(const BongoCatNeoI18n *i18n, uint32_t *ranges,
    size_t capacity);

#endif
