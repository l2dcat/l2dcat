#include "bongo/platform.h"

#if !defined(_WIN32) && !defined(__APPLE__)
#include "linux_update_key.h"
#include "linux_update_verify.h"
#include <openssl/evp.h>
#include <openssl/x509.h>
#include <stdio.h>
#include <string.h>

static int hex_value(char value) {
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    return -1;
}

static bool decode_signature(const char *text, unsigned char output[64]) {
    if (!text || strlen(text) != 128) return false;
    for (size_t i = 0; i < 64; ++i) {
        int high = hex_value(text[i * 2]), low = hex_value(text[i * 2 + 1]);
        if (high < 0 || low < 0) return false;
        output[i] = (unsigned char)((high << 4) | low);
    }
    return true;
}

bool bongo_linux_verify_signature(const unsigned char *key_data, size_t key_size,
    const char *payload, const char *signature) {
    unsigned char decoded[64];
    if (!key_data || !key_size || !payload || !decode_signature(signature, decoded))
        return false;
    const unsigned char *cursor = key_data;
    EVP_PKEY *key = d2i_PUBKEY(NULL, &cursor, (long)key_size);
    EVP_MD_CTX *context = key ? EVP_MD_CTX_new() : NULL;
    bool valid = key && context && EVP_PKEY_base_id(key) == EVP_PKEY_ED25519 &&
        EVP_DigestVerifyInit(context, NULL, NULL, NULL, key) == 1 &&
        EVP_DigestVerify(context, decoded, sizeof(decoded),
            (const unsigned char *)payload, strlen(payload)) == 1;
    if (context) EVP_MD_CTX_free(context);
    if (key) EVP_PKEY_free(key);
    return valid;
}

bool bongo_platform_verify_update(const char *path, const char *version,
    const char *platform, const char *sha256, uint64_t size,
    const char *signature, BongoError *error) {
    (void)path;
    if (!bongo_linux_update_key_size) {
        bongo_error_set(error, BONGO_ERROR_FORMAT,
            "Linux update public key is not configured");
        return false;
    }
    if (!signature || strlen(signature) != 128) {
        bongo_error_set(error, BONGO_ERROR_FORMAT, "Linux update signature is invalid");
        return false;
    }
    char payload[256];
    int length = snprintf(payload, sizeof(payload), "%s\n%s\n%s\n%llu\n",
        version, platform, sha256, (unsigned long long)size);
    bool valid = length > 0 && (size_t)length < sizeof(payload) &&
        bongo_linux_verify_signature(bongo_linux_update_key,
            bongo_linux_update_key_size, payload, signature);
    if (!valid) bongo_error_set(error, BONGO_ERROR_FORMAT,
        "Linux update signature verification failed");
    return valid;
}
#endif
