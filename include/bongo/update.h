#ifndef BONGO_UPDATE_H
#define BONGO_UPDATE_H

#include "bongo/common.h"

typedef struct BongoUpdater BongoUpdater;

typedef enum BongoUpdateStatus {
    BONGO_UPDATE_IDLE,
    BONGO_UPDATE_CHECKING,
    BONGO_UPDATE_CURRENT,
    BONGO_UPDATE_AVAILABLE,
    BONGO_UPDATE_DOWNLOADING,
    BONGO_UPDATE_READY,
    BONGO_UPDATE_FAILED
} BongoUpdateStatus;

typedef struct BongoUpdateSnapshot {
    BongoUpdateStatus status;
    char version[32];
    char notes[1024];
    char published[64];
    char message[256];
    uint64_t received;
    uint64_t total;
} BongoUpdateSnapshot;

BongoUpdater *bongo_update_create(const char *data_root);
void bongo_update_destroy(BongoUpdater *updater);
bool bongo_update_check(BongoUpdater *updater);
bool bongo_update_download(BongoUpdater *updater);
void bongo_update_snapshot(BongoUpdater *updater, BongoUpdateSnapshot *snapshot);
bool bongo_update_install(BongoUpdater *updater);

#endif
