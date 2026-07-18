#ifndef L2DCAT_TRAY_H
#define L2DCAT_TRAY_H

#include "l2dcat/common.h"

typedef struct L2DCatApp L2DCatApp;
typedef struct L2DCatTray L2DCatTray;

L2DCatTray *l2dcat_tray_create(L2DCatApp *app, L2DCatError *error);
void l2dcat_tray_destroy(L2DCatTray *tray);
void l2dcat_tray_sync(L2DCatTray *tray);
bool l2dcat_tray_self_test(L2DCatTray *tray);

#endif
