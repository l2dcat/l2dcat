#include "bongo/file.h"
#include "bongo/platform.h"

#ifdef _WIN32
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <windows.h>
#include <winhttp.h>

static wchar_t *wide(const char *text) {
    int length = text ? MultiByteToWideChar(CP_UTF8, 0, text, -1, NULL, 0) : 0;
    wchar_t *value = length ? malloc((size_t)length * sizeof(*value)) : NULL;
    if (value) MultiByteToWideChar(CP_UTF8, 0, text, -1, value, length);
    return value;
}

static void http_error(BongoError *error, const char *message) {
    bongo_error_set(error, BONGO_ERROR_IO, "%s (Windows error %lu)",
        message, (unsigned long)GetLastError());
}

static bool content_length(HINTERNET request, uint64_t *length) {
    wchar_t text[64]; DWORD size = sizeof(text);
    if (!WinHttpQueryHeaders(request, WINHTTP_QUERY_CONTENT_LENGTH,
        WINHTTP_HEADER_NAME_BY_INDEX, text, &size, WINHTTP_NO_HEADER_INDEX)) return false;
    *length = _wcstoui64(text, NULL, 10); return true;
}

static bool request_parts(wchar_t *url, wchar_t *host, size_t host_capacity,
    wchar_t *path, size_t path_capacity, INTERNET_PORT *port, BongoError *error) {
    URL_COMPONENTSW parts = {.dwStructSize = sizeof(parts), .dwHostNameLength = (DWORD)-1,
        .dwUrlPathLength = (DWORD)-1, .dwExtraInfoLength = (DWORD)-1};
    if (!WinHttpCrackUrl(url, 0, 0, &parts) || parts.nScheme != INTERNET_SCHEME_HTTPS) {
        bongo_error_set(error, BONGO_ERROR_FORMAT, "Update URL must use HTTPS"); return false;
    }
    if (parts.dwHostNameLength >= host_capacity ||
        parts.dwUrlPathLength + parts.dwExtraInfoLength >= path_capacity) {
        bongo_error_set(error, BONGO_ERROR_FORMAT, "Update URL is too long"); return false;
    }
    wmemcpy(host, parts.lpszHostName, parts.dwHostNameLength);
    host[parts.dwHostNameLength] = 0;
    wmemcpy(path, parts.lpszUrlPath, parts.dwUrlPathLength);
    if (parts.dwExtraInfoLength)
        wmemcpy(path + parts.dwUrlPathLength, parts.lpszExtraInfo, parts.dwExtraInfoLength);
    path[parts.dwUrlPathLength + parts.dwExtraInfoLength] = 0;
    *port = parts.nPort; return true;
}

BongoResult bongo_platform_download(const char *url, const char *destination,
    uint64_t limit, BongoDownloadProgress progress, void *userdata, BongoError *error) {
    if (!url || !destination) return BONGO_ERROR_ARGUMENT;
    wchar_t *url_wide = wide(url), host[256], path[BONGO_PATH_CAP]; INTERNET_PORT port;
    if (!url_wide || !request_parts(url_wide, host, 256, path, BONGO_PATH_CAP, &port, error)) {
        free(url_wide); return BONGO_ERROR_FORMAT;
    }
    HINTERNET session = WinHttpOpen(L"BongoCat/" L"1.1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    HINTERNET connection = session ? WinHttpConnect(session, host, port, 0) : NULL;
    HINTERNET request = connection ? WinHttpOpenRequest(connection, L"GET", path, NULL,
        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE) : NULL;
    DWORD policy = WINHTTP_OPTION_REDIRECT_POLICY_DISALLOW_HTTPS_TO_HTTP;
    if (request) WinHttpSetOption(request, WINHTTP_OPTION_REDIRECT_POLICY, &policy, sizeof(policy));
    bool ok = request && WinHttpSetTimeouts(request, 5000, 5000, 5000, 15000) &&
        WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
            WINHTTP_NO_REQUEST_DATA, 0, 0, 0) && WinHttpReceiveResponse(request, NULL);
    DWORD status = 0, status_size = sizeof(status);
    if (ok) {
        ok = WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE |
            WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status,
            &status_size, WINHTTP_NO_HEADER_INDEX) != FALSE;
        if (ok && status != 200) {
            bongo_error_set(error, BONGO_ERROR_IO, "Update server returned HTTP %lu",
                (unsigned long)status);
            ok = false;
        }
    }
    uint64_t total = 0, received = 0;
    if (ok && content_length(request, &total) && limit && total > limit) {
        bongo_error_set(error, BONGO_ERROR_IO, "Update download exceeds size limit"); ok = false;
    }
    FILE *file = ok ? bongo_file_open(destination, "wb") : NULL;
    if (ok && !file) { bongo_error_set(error, BONGO_ERROR_IO, "Cannot create update file"); ok = false; }
    unsigned char buffer[65536];
    while (ok) {
        DWORD count = 0;
        if (!WinHttpReadData(request, buffer, sizeof(buffer), &count)) { ok = false; break; }
        if (!count) break;
        received += count;
        if ((limit && received > limit) || fwrite(buffer, 1, count, file) != count) {
            bongo_error_set(error, BONGO_ERROR_IO, "Cannot write update download"); ok = false; break;
        }
        if (progress) progress(received, total, userdata);
    }
    if (file && fclose(file) != 0) ok = false;
    if (!ok) {
        bongo_file_remove(destination);
        if (!error || !error->message[0]) http_error(error, "Update download failed");
    }
    if (request) WinHttpCloseHandle(request);
    if (connection) WinHttpCloseHandle(connection);
    if (session) WinHttpCloseHandle(session);
    free(url_wide);
    return ok ? BONGO_OK : BONGO_ERROR_IO;
}
#endif
