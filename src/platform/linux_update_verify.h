#ifndef BONGO_LINUX_UPDATE_VERIFY_H
#define BONGO_LINUX_UPDATE_VERIFY_H

#include <stdbool.h>
#include <stddef.h>

bool bongo_linux_verify_signature(const unsigned char *key_data, size_t key_size,
    const char *payload, const char *signature);

#endif
