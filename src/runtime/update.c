#include "l2dcat/update.h"
#include "l2dcat/file.h"
#include "l2dcat/path.h"
#include "l2dcat/platform.h"
#include "l2dcat/sha256.h"
#include "l2dcat/update_manifest.h"

#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct L2DCatUpdater {
    SDL_Mutex *mutex;
    SDL_Thread *thread;
    bool running;
    int command;
    char data_root[L2DCAT_PATH_CAP];
    char staged[L2DCAT_PATH_CAP];
    L2DCatUpdateManifest manifest;
    L2DCatUpdateSnapshot state;
};

static const char *platform_key(void) {
#if defined(_WIN32) && (defined(_M_ARM64) || defined(__aarch64__))
    return "windows-aarch64";
#elif defined(_WIN32)
    return sizeof(void *) == 8 ? "windows-x86_64" : "windows-i686";
#elif defined(__APPLE__) && defined(__aarch64__)
    return "darwin-aarch64";
#elif defined(__APPLE__)
    return "darwin-x86_64";
#elif defined(__aarch64__)
    return "linux-aarch64";
#else
    return "linux-x86_64";
#endif
}

static void set_error(L2DCatUpdater *value, const L2DCatError *error) {
    SDL_LockMutex(value->mutex);
    value->state.status = L2DCAT_UPDATE_FAILED;
    snprintf(value->state.message, sizeof(value->state.message), "%s",
        error && error->message[0] ? error->message : "Update failed");
    SDL_UnlockMutex(value->mutex);
    SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Update: %s",
        error && error->message[0] ? error->message : "Update failed");
}

static void progress(uint64_t received, uint64_t total, void *userdata) {
    L2DCatUpdater *value = userdata;
    SDL_LockMutex(value->mutex);
    value->state.received = received; value->state.total = total;
    SDL_UnlockMutex(value->mutex);
}

static void finish_check(L2DCatUpdater *value, const L2DCatUpdateManifest *manifest) {
    SDL_LockMutex(value->mutex);
    value->manifest = *manifest;
    snprintf(value->state.version, sizeof(value->state.version), "%s", manifest->version);
    snprintf(value->state.notes, sizeof(value->state.notes), "%s", manifest->notes);
    snprintf(value->state.published, sizeof(value->state.published), "%s", manifest->published);
    value->state.status = l2dcat_version_compare(manifest->version, L2DCAT_VERSION) > 0
        ? L2DCAT_UPDATE_AVAILABLE : L2DCAT_UPDATE_CURRENT;
    value->state.message[0] = '\0';
    SDL_UnlockMutex(value->mutex);
    SDL_Log("Update manifest %s: %s", manifest->version,
        l2dcat_version_compare(manifest->version, L2DCAT_VERSION) > 0 ? "available" : "current");
}

static void check_update(L2DCatUpdater *value) {
    char path[L2DCAT_PATH_CAP]; L2DCatError error = {0}; L2DCatUpdateManifest manifest;
    l2dcat_path_join(path, sizeof(path), value->data_root, "latest-native.json.part");
    const char *override = SDL_getenv("L2DCAT_UPDATE_URL");
    char default_url[L2DCAT_PATH_CAP];
    snprintf(default_url, sizeof(default_url), "%s%s%s.json",
        L2DCAT_UPDATE_MANIFEST_BASE_URL,
        L2DCAT_UPDATE_MANIFEST_BASE_URL[0] &&
            L2DCAT_UPDATE_MANIFEST_BASE_URL[strlen(L2DCAT_UPDATE_MANIFEST_BASE_URL) - 1] == '/'
            ? "" : "/", platform_key());
    const char *url = override && override[0] ? override :
        L2DCAT_UPDATE_MANIFEST_BASE_URL[0] ? default_url : NULL;
    if (!url) {
        l2dcat_error_set(&error, L2DCAT_ERROR_PLATFORM,
            "Updates are not configured for this independent build");
        set_error(value, &error); return;
    }
    L2DCatResult result = l2dcat_platform_download(url,
        path, 1024 * 1024, NULL, NULL, &error);
    if (result == L2DCAT_OK) result = l2dcat_update_manifest_load(path,
        platform_key(), &manifest, &error);
    l2dcat_file_remove(path);
    if (result == L2DCAT_OK) finish_check(value, &manifest); else set_error(value, &error);
}

static void download_update(L2DCatUpdater *value) {
    L2DCatUpdateManifest manifest;
    SDL_LockMutex(value->mutex); manifest = value->manifest; SDL_UnlockMutex(value->mutex);
    L2DCatError error = {0}; char hash[65] = {0};
#if defined(_WIN32)
    const char *staged_name = "l2dcat-update.exe.part";
#elif defined(__APPLE__)
    const char *staged_name = "l2dcat-update.app.zip.part";
#else
    const char *staged_name = "l2dcat-update.AppImage.part";
#endif
    l2dcat_path_join(value->staged, sizeof(value->staged), value->data_root,
        staged_name);
    uint64_t limit = manifest.size ? manifest.size + 1024 : 256ull * 1024 * 1024;
    L2DCatResult result = l2dcat_platform_download(manifest.url, value->staged,
        limit, progress, value, &error);
    if (result == L2DCAT_OK) result = l2dcat_sha256_file(value->staged, hash, &error);
    if (result == L2DCAT_OK && strcmp(hash, manifest.sha256) != 0) {
        l2dcat_error_set(&error, L2DCAT_ERROR_FORMAT, "Downloaded update SHA-256 does not match");
        result = L2DCAT_ERROR_FORMAT;
    }
    if (result == L2DCAT_OK && !l2dcat_platform_verify_update(value->staged,
        manifest.version, platform_key(), hash, manifest.size,
        manifest.signature, &error)) result = L2DCAT_ERROR_FORMAT;
    if (result != L2DCAT_OK) {
        l2dcat_file_remove(value->staged); set_error(value, &error); return;
    }
    SDL_LockMutex(value->mutex); value->state.status = L2DCAT_UPDATE_READY;
    value->state.message[0] = '\0'; SDL_UnlockMutex(value->mutex);
    SDL_Log("Update download verified and ready");
}

static int SDLCALL worker(void *userdata) {
    L2DCatUpdater *value = userdata;
    if (value->command == 1) check_update(value); else download_update(value);
    SDL_LockMutex(value->mutex); value->running = false; SDL_UnlockMutex(value->mutex);
    return 0;
}

static void reap(L2DCatUpdater *value) {
    SDL_Thread *thread = NULL;
    SDL_LockMutex(value->mutex);
    if (value->thread && !value->running) { thread = value->thread; value->thread = NULL; }
    SDL_UnlockMutex(value->mutex);
    if (thread) SDL_WaitThread(thread, NULL);
}

static bool start(L2DCatUpdater *value, int command, L2DCatUpdateStatus status) {
    if (!value) return false;
    reap(value);
    SDL_LockMutex(value->mutex);
    if (value->thread) { SDL_UnlockMutex(value->mutex); return false; }
    value->command = command; value->running = true; value->state.status = status;
    value->state.message[0] = '\0'; value->state.received = value->state.total = 0;
    value->thread = SDL_CreateThread(worker, "l2dcat-update", value);
    if (!value->thread) {
        value->running = false; value->state.status = L2DCAT_UPDATE_FAILED;
        snprintf(value->state.message, sizeof(value->state.message), "%s", SDL_GetError());
    }
    bool ok = value->thread != NULL; SDL_UnlockMutex(value->mutex); return ok;
}

L2DCatUpdater *l2dcat_update_create(const char *data_root) {
    if (!data_root) return NULL;
    L2DCatUpdater *value = calloc(1, sizeof(*value));
    if (!value) return NULL;
    value->mutex = SDL_CreateMutex();
    if (!value->mutex) { free(value); return NULL; }
    snprintf(value->data_root, sizeof(value->data_root), "%s", data_root); return value;
}

void l2dcat_update_destroy(L2DCatUpdater *value) {
    if (!value) return;
    if (value->thread) SDL_WaitThread(value->thread, NULL);
    if (value->mutex) SDL_DestroyMutex(value->mutex);
    free(value);
}

bool l2dcat_update_check(L2DCatUpdater *value) {
    return start(value, 1, L2DCAT_UPDATE_CHECKING);
}

bool l2dcat_update_download(L2DCatUpdater *value) {
    L2DCatUpdateSnapshot state; l2dcat_update_snapshot(value, &state);
    return state.status == L2DCAT_UPDATE_AVAILABLE &&
        start(value, 2, L2DCAT_UPDATE_DOWNLOADING);
}

void l2dcat_update_snapshot(L2DCatUpdater *value, L2DCatUpdateSnapshot *snapshot) {
    if (!snapshot) return;
    memset(snapshot, 0, sizeof(*snapshot));
    if (!value) return;
    reap(value);
    SDL_LockMutex(value->mutex); *snapshot = value->state; SDL_UnlockMutex(value->mutex);
}

bool l2dcat_update_install(L2DCatUpdater *value) {
    L2DCatUpdateSnapshot state; l2dcat_update_snapshot(value, &state);
    if (!value || state.status != L2DCAT_UPDATE_READY) return false;
    L2DCatError error = {0};
    if (l2dcat_platform_schedule_update(value->staged, &error)) return true;
    set_error(value, &error); return false;
}
