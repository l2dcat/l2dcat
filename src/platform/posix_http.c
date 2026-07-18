#include "l2dcat/file.h"
#include "l2dcat/platform.h"

#ifndef _WIN32
#include <curl/curl.h>
#include <stdio.h>
#include <string.h>

typedef struct DownloadState {
    FILE *file;
    uint64_t received;
    uint64_t total;
    uint64_t limit;
    L2DCatDownloadProgress progress;
    void *userdata;
    bool overflow;
    bool write_failed;
} DownloadState;

static size_t write_data(char *data, size_t size, size_t count, void *userdata) {
    DownloadState *state = userdata;
    size_t bytes = size * count;
    if (state->limit && state->received + bytes > state->limit) {
        state->overflow = true;
        return 0;
    }
    if (fwrite(data, 1, bytes, state->file) != bytes) {
        state->write_failed = true;
        return 0;
    }
    state->received += bytes;
    if (state->progress) state->progress(state->received, state->total, state->userdata);
    return bytes;
}

static int transfer(void *userdata, curl_off_t download_total,
    curl_off_t download_now, curl_off_t upload_total, curl_off_t upload_now) {
    (void)download_now; (void)upload_total; (void)upload_now;
    DownloadState *state = userdata;
    state->total = download_total > 0 ? (uint64_t)download_total : 0;
    if (state->limit && state->total > state->limit) {
        state->overflow = true;
        return 1;
    }
    return 0;
}

L2DCatResult l2dcat_platform_download(const char *url, const char *destination,
    uint64_t limit, L2DCatDownloadProgress progress, void *userdata, L2DCatError *error) {
    if (!url || !destination) return L2DCAT_ERROR_ARGUMENT;
    if (strncmp(url, "https://", 8) != 0) {
        l2dcat_error_set(error, L2DCAT_ERROR_FORMAT, "Update URL must use HTTPS");
        return L2DCAT_ERROR_FORMAT;
    }
    FILE *file = l2dcat_file_open(destination, "wb");
    if (!file) {
        l2dcat_error_set(error, L2DCAT_ERROR_IO, "Cannot create update file");
        return L2DCAT_ERROR_IO;
    }
    DownloadState state = {file, 0, 0, limit, progress, userdata, false, false};
    CURLcode initialized = curl_global_init(CURL_GLOBAL_DEFAULT);
    CURL *request = initialized == CURLE_OK ? curl_easy_init() : NULL;
    if (request) {
        curl_easy_setopt(request, CURLOPT_URL, url);
        curl_easy_setopt(request, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(request, CURLOPT_MAXREDIRS, 5L);
#if LIBCURL_VERSION_NUM >= 0x075500
        curl_easy_setopt(request, CURLOPT_PROTOCOLS_STR, "https");
        curl_easy_setopt(request, CURLOPT_REDIR_PROTOCOLS_STR, "https");
#else
        curl_easy_setopt(request, CURLOPT_PROTOCOLS, CURLPROTO_HTTPS);
        curl_easy_setopt(request, CURLOPT_REDIR_PROTOCOLS, CURLPROTO_HTTPS);
#endif
        curl_easy_setopt(request, CURLOPT_CONNECTTIMEOUT, 10L);
        curl_easy_setopt(request, CURLOPT_TIMEOUT, 120L);
        curl_easy_setopt(request, CURLOPT_FAILONERROR, 1L);
        curl_easy_setopt(request, CURLOPT_USERAGENT, "l2dcat/" L2DCAT_VERSION);
        curl_easy_setopt(request, CURLOPT_WRITEFUNCTION, write_data);
        curl_easy_setopt(request, CURLOPT_WRITEDATA, &state);
        curl_easy_setopt(request, CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(request, CURLOPT_XFERINFOFUNCTION, transfer);
        curl_easy_setopt(request, CURLOPT_XFERINFODATA, &state);
    }
    CURLcode result = request ? curl_easy_perform(request) : CURLE_FAILED_INIT;
    if (request) curl_easy_cleanup(request);
    curl_global_cleanup();
    bool closed = fclose(file) == 0;
    if (result == CURLE_OK && closed && !state.overflow && !state.write_failed)
        return L2DCAT_OK;
    l2dcat_file_remove(destination);
    if (state.overflow) l2dcat_error_set(error, L2DCAT_ERROR_IO,
        "Update download exceeds size limit");
    else if (state.write_failed || !closed) l2dcat_error_set(error, L2DCAT_ERROR_IO,
        "Cannot write update download");
    else l2dcat_error_set(error, L2DCAT_ERROR_IO, "Update download failed: %s",
        curl_easy_strerror(result));
    return L2DCAT_ERROR_IO;
}
#endif
