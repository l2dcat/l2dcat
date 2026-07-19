#include "l2dcat/memory.h"

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#elif defined(__APPLE__)
#include <malloc/malloc.h>
#elif defined(__GLIBC__)
#include <malloc.h>
#endif

void l2dcat_platform_trim_memory(void) {
#ifdef _WIN32
    EmptyWorkingSet(GetCurrentProcess());
#elif defined(__APPLE__)
    malloc_zone_pressure_relief(NULL, 0);
#elif defined(__GLIBC__)
    malloc_trim(0);
#endif
}
