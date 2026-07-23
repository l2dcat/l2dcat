#ifndef BONGO_CAT_NEO_TRAY_H
#define BONGO_CAT_NEO_TRAY_H

#include "bongo_cat_neo/common.h"

typedef struct BongoCatNeoApp BongoCatNeoApp;
typedef struct BongoCatNeoTray BongoCatNeoTray;

BongoCatNeoTray *bongo_cat_neo_tray_create(BongoCatNeoApp *app, BongoCatNeoError *error);
void bongo_cat_neo_tray_destroy(BongoCatNeoTray *tray);
void bongo_cat_neo_tray_sync(BongoCatNeoTray *tray);
bool bongo_cat_neo_tray_self_test(BongoCatNeoTray *tray);

#endif
