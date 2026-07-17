#include "bongo/platform.h"

#ifdef _WIN32
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <windows.h>
#include <wincrypt.h>
#include <wintrust.h>
#include <shellapi.h>
#include <softpub.h>

static wchar_t *wide(const char *text) {
    int length = text ? MultiByteToWideChar(CP_UTF8, 0, text, -1, NULL, 0) : 0;
    wchar_t *value = length ? malloc((size_t)length * sizeof(*value)) : NULL;
    if (value) MultiByteToWideChar(CP_UTF8, 0, text, -1, value, length);
    return value;
}

static bool launch(wchar_t *executable, wchar_t *command) {
    STARTUPINFOW startup = {.cb = sizeof(startup)}; PROCESS_INFORMATION process = {0};
    bool ok = CreateProcessW(executable, command, NULL, NULL, FALSE,
        CREATE_NO_WINDOW, NULL, NULL, &startup, &process) != FALSE;
    if (ok) { CloseHandle(process.hThread); CloseHandle(process.hProcess); }
    return ok;
}

static bool trusted_file(const wchar_t *path) {
    WINTRUST_FILE_INFO file = {sizeof(file), path, NULL, NULL};
    WINTRUST_DATA data = {0};
    data.cbStruct = sizeof(data);
    data.dwUIChoice = WTD_UI_NONE;
    data.fdwRevocationChecks = WTD_REVOKE_WHOLECHAIN;
    data.dwUnionChoice = WTD_CHOICE_FILE;
    data.pFile = &file;
    data.dwStateAction = WTD_STATEACTION_VERIFY;
    data.dwProvFlags = WTD_REVOCATION_CHECK_CHAIN_EXCLUDE_ROOT;
    GUID policy = WINTRUST_ACTION_GENERIC_VERIFY_V2;
    LONG status = WinVerifyTrust(NULL, &policy, &data);
    data.dwStateAction = WTD_STATEACTION_CLOSE;
    WinVerifyTrust(NULL, &policy, &data);
    return status == ERROR_SUCCESS;
}

static PCCERT_CONTEXT signer_certificate(const wchar_t *path,
    HCERTSTORE *store, HCRYPTMSG *message) {
    DWORD encoding = 0, content = 0, format = 0;
    if (!CryptQueryObject(CERT_QUERY_OBJECT_FILE, path,
        CERT_QUERY_CONTENT_FLAG_PKCS7_SIGNED_EMBED,
        CERT_QUERY_FORMAT_FLAG_BINARY, 0, &encoding, &content, &format,
        store, message, NULL)) return NULL;
    DWORD size = 0;
    if (!CryptMsgGetParam(*message, CMSG_SIGNER_INFO_PARAM, 0, NULL, &size)) return NULL;
    CMSG_SIGNER_INFO *signer = calloc(1, size);
    if (!signer || !CryptMsgGetParam(*message, CMSG_SIGNER_INFO_PARAM,
        0, signer, &size)) { free(signer); return NULL; }
    CERT_INFO info = {0};
    info.Issuer = signer->Issuer;
    info.SerialNumber = signer->SerialNumber;
    PCCERT_CONTEXT certificate = CertFindCertificateInStore(*store,
        encoding, 0, CERT_FIND_SUBJECT_CERT, &info, NULL);
    free(signer);
    return certificate;
}

static bool same_publisher(const wchar_t *candidate, const wchar_t *current) {
    if (!trusted_file(candidate) || !trusted_file(current)) return false;
    HCERTSTORE candidate_store = NULL, current_store = NULL;
    HCRYPTMSG candidate_message = NULL, current_message = NULL;
    PCCERT_CONTEXT candidate_cert = signer_certificate(candidate,
        &candidate_store, &candidate_message);
    PCCERT_CONTEXT current_cert = signer_certificate(current,
        &current_store, &current_message);
    bool matches = candidate_cert && current_cert && CertComparePublicKeyInfo(
        X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
        &candidate_cert->pCertInfo->SubjectPublicKeyInfo,
        &current_cert->pCertInfo->SubjectPublicKeyInfo);
    if (candidate_cert) CertFreeCertificateContext(candidate_cert);
    if (current_cert) CertFreeCertificateContext(current_cert);
    if (candidate_message) CryptMsgClose(candidate_message);
    if (current_message) CryptMsgClose(current_message);
    if (candidate_store) CertCloseStore(candidate_store, 0);
    if (current_store) CertCloseStore(current_store, 0);
    return matches;
}

static int apply_update(wchar_t **arguments) {
    const wchar_t *staged = arguments[2], *target = arguments[3];
    DWORD parent_id = wcstoul(arguments[4], NULL, 10);
    HANDLE parent = OpenProcess(SYNCHRONIZE, FALSE, parent_id);
    if (parent) { WaitForSingleObject(parent, 60000); CloseHandle(parent); }
    if (!same_publisher(staged, target)) return 1;
    wchar_t backup[BONGO_PATH_CAP];
    int backup_length = swprintf(backup, BONGO_PATH_CAP, L"%ls.old", target);
    if (backup_length <= 0 || backup_length >= BONGO_PATH_CAP) return 1;
    DeleteFileW(backup);
    bool backed_up = false;
    for (int i = 0; i < 100 && !backed_up; ++i) {
        backed_up = MoveFileExW(target, backup,
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
        if (!backed_up) Sleep(100);
    }
    if (!backed_up) return 1;
    if (!MoveFileExW(staged, target, MOVEFILE_WRITE_THROUGH)) {
        MoveFileExW(backup, target, MOVEFILE_WRITE_THROUGH); return 1;
    }
    wchar_t self[BONGO_PATH_CAP], command[BONGO_PATH_CAP * 3 + 96];
    if (!GetModuleFileNameW(NULL, self, BONGO_PATH_CAP)) {
        DeleteFileW(target); MoveFileExW(backup, target, MOVEFILE_WRITE_THROUGH); return 1;
    }
    swprintf(command, sizeof(command) / sizeof(*command),
        L"\"%ls\" --bongo-cleanup-helper \"%ls\" \"%ls\"",
        target, self, backup);
    if (launch((wchar_t *)target, command)) return 0;
    DeleteFileW(target); MoveFileExW(backup, target, MOVEFILE_WRITE_THROUGH); return 1;
}

static void cleanup_helper(const wchar_t *path) {
    for (int i = 0; i < 100; ++i) {
        if (DeleteFileW(path) || GetLastError() == ERROR_FILE_NOT_FOUND) return;
        Sleep(50);
    }
    MoveFileExW(path, NULL, MOVEFILE_DELAY_UNTIL_REBOOT);
}

int bongo_platform_update_helper(int argc, char **argv) {
    (void)argc; (void)argv;
    int count = 0; wchar_t **arguments = CommandLineToArgvW(GetCommandLineW(), &count);
    if (!arguments) return -1;
    int result = -1;
    if (count == 5 && wcscmp(arguments[1], L"--bongo-apply-update") == 0)
        result = apply_update(arguments);
    else if (count == 4 && wcscmp(arguments[1], L"--bongo-cleanup-helper") == 0) {
        cleanup_helper(arguments[2]); cleanup_helper(arguments[3]);
    }
    LocalFree(arguments); return result;
}

bool bongo_platform_schedule_update(const char *staged, BongoError *error) {
    wchar_t target[BONGO_PATH_CAP], helper[BONGO_PATH_CAP];
    wchar_t *staged_wide = wide(staged);
    if (!staged_wide || !GetModuleFileNameW(NULL, target, BONGO_PATH_CAP)) {
        free(staged_wide); return false;
    }
    if (!same_publisher(staged_wide, target)) {
        bongo_error_set(error, BONGO_ERROR_FORMAT,
            "Windows update must be trusted and signed by the current publisher");
        free(staged_wide); return false;
    }
    int length = swprintf(helper, BONGO_PATH_CAP, L"%ls.update-helper.exe", staged_wide);
    if (length <= 0 || length >= BONGO_PATH_CAP || !CopyFileW(target, helper, FALSE)) {
        bongo_error_set(error, BONGO_ERROR_IO, "Cannot create update helper");
        free(staged_wide); return false;
    }
    wchar_t command[BONGO_PATH_CAP * 3 + 96];
    swprintf(command, sizeof(command) / sizeof(*command),
        L"\"%ls\" --bongo-apply-update \"%ls\" \"%ls\" %lu",
        helper, staged_wide, target, (unsigned long)GetCurrentProcessId());
    bool ok = launch(helper, command);
    if (!ok) {
        DeleteFileW(helper);
        bongo_error_set(error, BONGO_ERROR_PLATFORM, "Cannot launch update helper");
    }
    free(staged_wide); return ok;
}

bool bongo_platform_verify_update(const char *path, const char *version,
    const char *platform, const char *sha256, uint64_t size,
    const char *signature, BongoError *error) {
    (void)path; (void)version; (void)platform; (void)sha256;
    (void)size; (void)signature; (void)error; return true;
}
#endif
