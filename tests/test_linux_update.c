#include "test.h"
#include "linux_update_verify.h"

#include <openssl/evp.h>
#include <openssl/x509.h>
#include <stdio.h>
#include <string.h>

int bongo_test_failures;

static EVP_PKEY *generate_key(void) {
    EVP_PKEY_CTX *context = EVP_PKEY_CTX_new_id(EVP_PKEY_ED25519, NULL);
    EVP_PKEY *key = NULL;
    if (!context || EVP_PKEY_keygen_init(context) != 1 ||
        EVP_PKEY_keygen(context, &key) != 1) {
        if (key) EVP_PKEY_free(key);
        key = NULL;
    }
    if (context) EVP_PKEY_CTX_free(context);
    return key;
}

static bool sign_payload(EVP_PKEY *key, const char *payload, char output[129]) {
    EVP_MD_CTX *context = EVP_MD_CTX_new();
    unsigned char signature[64]; size_t size = sizeof(signature);
    bool signed_ok = context && EVP_DigestSignInit(context, NULL, NULL, NULL, key) == 1 &&
        EVP_DigestSign(context, signature, &size,
            (const unsigned char *)payload, strlen(payload)) == 1 && size == 64;
    if (context) EVP_MD_CTX_free(context);
    if (!signed_ok) return false;
    for (size_t i = 0; i < size; ++i) snprintf(output + i * 2, 3, "%02x", signature[i]);
    output[128] = '\0'; return true;
}

int main(void) {
    const char *payload = "1.1.0\nlinux-x86_64\n"
        "cd133a7cd93447065d364d6bc48f8fcd27ffd86dd9b16c75d6758312a360a2b0\n20\n";
    EVP_PKEY *key = generate_key();
    CHECK(key != NULL);
    int der_size = key ? i2d_PUBKEY(key, NULL) : 0;
    unsigned char *der = der_size > 0 ? OPENSSL_malloc((size_t)der_size) : NULL;
    unsigned char *cursor = der;
    CHECK(der && i2d_PUBKEY(key, &cursor) == der_size);
    char signature[129];
    CHECK(key && sign_payload(key, payload, signature));
    CHECK(bongo_linux_verify_signature(der, (size_t)der_size, payload, signature));
    CHECK(!bongo_linux_verify_signature(der, (size_t)der_size, "tampered", signature));
    signature[0] = signature[0] == '0' ? '1' : '0';
    CHECK(!bongo_linux_verify_signature(der, (size_t)der_size, payload, signature));
    signature[0] = 'g';
    CHECK(!bongo_linux_verify_signature(der, (size_t)der_size, payload, signature));
    OPENSSL_free(der);
    if (key) EVP_PKEY_free(key);
    return bongo_test_failures ? 1 : 0;
}
