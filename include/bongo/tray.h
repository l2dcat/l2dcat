#ifndef BONGO_TRAY_H
#define BONGO_TRAY_H

#include "bongo/common.h"

typedef struct BongoApp BongoApp;
typedef struct BongoTray BongoTray;

BongoTray *bongo_tray_create(BongoApp *app, BongoError *error);
void bongo_tray_destroy(BongoTray *tray);
void bongo_tray_sync(BongoTray *tray);
bool bongo_tray_self_test(BongoTray *tray);

#endif
