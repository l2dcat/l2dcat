#include "l2dcat/common.h"

#include <stdarg.h>
#include <stdio.h>

void l2dcat_error_set(L2DCatError *error, L2DCatResult code, const char *format, ...) {
    if (!error) return;
    error->code = code;
    if (!format) {
        error->message[0] = '\0';
        return;
    }
    va_list args;
    va_start(args, format);
    vsnprintf(error->message, sizeof(error->message), format, args);
    va_end(args);
}
