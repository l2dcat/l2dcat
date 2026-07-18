#include "test.h"
#include "l2dcat/file.h"
#include "l2dcat/sha256.h"
#include "l2dcat/update_manifest.h"

#include <stdio.h>
#include <string.h>

static void write_manifest(const char *path, const char *version,
    const char *url, const char *sha256) {
    FILE *file = l2dcat_file_open(path, "wb");
    CHECK(file != NULL);
    if (!file) return;
    fprintf(file, "{\"version\":\"%s\",\"notes\":\"fix\",\"platforms\":{"
        "\"windows-x86_64\":{\"url\":\"%s\",\"sha256\":\"%s\"}}}",
        version, url, sha256);
    fclose(file);
}

void test_update(void) {
    CHECK(l2dcat_version_compare("1.2.0", "1.1.9") > 0);
    CHECK(l2dcat_version_compare("v1.1.0", "1.1") == 0);
    CHECK(l2dcat_version_compare("1.1.0-native", "1.1.0") < 0);
    CHECK(l2dcat_version_compare("1.1.0-beta.10", "1.1.0-beta.2") > 0);
    CHECK(l2dcat_version_compare("1.1.0-alpha", "1.1.0-alpha.1") < 0);
    CHECK(l2dcat_version_compare("1.1.0+build.2", "1.1.0+build.1") == 0);
    CHECK(l2dcat_version_compare("invalid", "1.1.0") == 0);
    char hash[65];
    l2dcat_sha256_bytes("abc", 3, hash);
    CHECK(strcmp(hash, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad") == 0);

    const char *path = "l2dcat-\xE6\x9B\xB4\xE6\x96\xB0.json";
    const char *valid_hash =
        "BA7816BF8F01CFEA414140DE5DAE2223B00361A396177A9CB410FF61F20015AD";
    write_manifest(path, "1.2.0", "https://example.test/l2dcat.exe", valid_hash);
    L2DCatUpdateManifest manifest; L2DCatError error = {0};
    CHECK(l2dcat_update_manifest_load(path, "windows-x86_64", &manifest, &error) == L2DCAT_OK);
    CHECK(strcmp(manifest.version, "1.2.0") == 0);
    CHECK(strcmp(manifest.notes, "fix") == 0);
    CHECK(manifest.sha256[0] == 'b');

    write_manifest(path, "invalid", "https://example.test/l2dcat.exe", valid_hash);
    CHECK(l2dcat_update_manifest_load(path, "windows-x86_64", &manifest, &error) == L2DCAT_ERROR_FORMAT);
    write_manifest(path, "1.2.0", "http://example.test/l2dcat.exe", valid_hash);
    CHECK(l2dcat_update_manifest_load(path, "windows-x86_64", &manifest, &error) == L2DCAT_ERROR_FORMAT);
    write_manifest(path, "1.2.0", "https://", valid_hash);
    CHECK(l2dcat_update_manifest_load(path, "windows-x86_64", &manifest, &error) == L2DCAT_ERROR_FORMAT);
    write_manifest(path, "1.2.0", "https://user@example.test/update", valid_hash);
    CHECK(l2dcat_update_manifest_load(path, "windows-x86_64", &manifest, &error) == L2DCAT_ERROR_FORMAT);
    write_manifest(path, "1.2.0", "https://example.test/update#ignored", valid_hash);
    CHECK(l2dcat_update_manifest_load(path, "windows-x86_64", &manifest, &error) == L2DCAT_ERROR_FORMAT);
    write_manifest(path, "1.2.0", "https://example.test/l2dcat.exe", "not-a-sha256");
    CHECK(l2dcat_update_manifest_load(path, "windows-x86_64", &manifest, &error) == L2DCAT_ERROR_FORMAT);
    CHECK(l2dcat_file_remove(path));
}
