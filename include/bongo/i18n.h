#ifndef BONGO_I18N_H
#define BONGO_I18N_H

#include "bongo/config.h"

typedef struct BongoI18n BongoI18n;

BongoI18n *bongo_i18n_create(const char *root, BongoLanguage language, BongoError *error);
void bongo_i18n_destroy(BongoI18n *i18n);
BongoResult bongo_i18n_reload(BongoI18n *i18n, BongoLanguage language,
    BongoError *error);
const char *bongo_i18n_get(const BongoI18n *i18n, const char *key,
    const char *fallback);
size_t bongo_i18n_glyph_ranges(const BongoI18n *i18n, uint32_t *ranges,
    size_t capacity);

#endif
