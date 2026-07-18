#include "ui_font.h"
#include "l2dcat/file.h"
#include "l2dcat/path.h"

#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>

static bool readable(const char *path) {
    FILE *file = l2dcat_file_open(path, "rb");
    if (!file) return false;
    fclose(file);
    return true;
}

const char *l2dcat_ui_system_font(char *path, size_t capacity, bool multilingual) {
#ifdef _WIN32
    const char *windows = getenv("WINDIR");
    if (!windows) windows = getenv("SystemRoot");
    if (windows) {
        const char *multi[] = {"Fonts/msyhl.ttc", "Fonts/msyh.ttc"};
        const char *latin[] = {"Fonts/segoeui.ttf", "Fonts/msyhl.ttc"};
        const char **candidates = multilingual ? multi : latin;
        for (size_t i = 0; i < 2; ++i) {
            l2dcat_path_join(path, capacity, windows, candidates[i]);
            if (readable(path)) return path;
        }
    }
#elif defined(__APPLE__)
    const char *candidates[] = {multilingual ? "/System/Library/Fonts/PingFang.ttc" :
        "/System/Library/Fonts/Helvetica.ttc", "/System/Library/Fonts/PingFang.ttc"};
#else
    const char *candidates[] = {multilingual ?
        "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc" :
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc"};
#endif
#ifndef _WIN32
    for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); ++i) {
        if (!readable(candidates[i])) continue;
        snprintf(path, capacity, "%s", candidates[i]);
        return path;
    }
#endif
    return NULL;
}
