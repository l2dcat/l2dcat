#ifndef L2DCAT_UPDATE_H
#define L2DCAT_UPDATE_H

#include "l2dcat/common.h"

typedef struct L2DCatUpdater L2DCatUpdater;

typedef enum L2DCatUpdateStatus {
    L2DCAT_UPDATE_IDLE,
    L2DCAT_UPDATE_CHECKING,
    L2DCAT_UPDATE_CURRENT,
    L2DCAT_UPDATE_AVAILABLE,
    L2DCAT_UPDATE_DOWNLOADING,
    L2DCAT_UPDATE_READY,
    L2DCAT_UPDATE_FAILED
} L2DCatUpdateStatus;

typedef struct L2DCatUpdateSnapshot {
    L2DCatUpdateStatus status;
    char version[32];
    char notes[1024];
    char published[64];
    char message[256];
    uint64_t received;
    uint64_t total;
} L2DCatUpdateSnapshot;

L2DCatUpdater *l2dcat_update_create(const char *data_root);
void l2dcat_update_destroy(L2DCatUpdater *updater);
bool l2dcat_update_check(L2DCatUpdater *updater);
bool l2dcat_update_download(L2DCatUpdater *updater);
void l2dcat_update_snapshot(L2DCatUpdater *updater, L2DCatUpdateSnapshot *snapshot);
bool l2dcat_update_install(L2DCatUpdater *updater);

#endif
