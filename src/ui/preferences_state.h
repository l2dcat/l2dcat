#ifndef L2DCAT_PREFERENCES_STATE_H
#define L2DCAT_PREFERENCES_STATE_H

#include "preferences_internal.h"
#include "ui_backend.h"
#include "l2dcat/i18n.h"
#include "l2dcat/preferences.h"

struct L2DCatPreferences {
    L2DCatApp *app;
    SDL_Window *window;
    SDL_GLContext gl_context;
    bool owns_gl_context;
    L2DCatUIBackend ui;
    int page;
    int style_theme;
    L2DCatLanguage font_language;
    nk_rune glyph_ranges[2048];
    bool input_active;
    bool import_requested;
    L2DCatImportDialog *import_dialog;
    bool frame_checked;
    bool render_dirty;
    uint64_t last_render_ns;
    float scroll_current[5];
    float scroll_target[5];
    bool scroll_ready[5];
    char notice[384];
    uint64_t notice_until_ns;
    bool notice_error;
    bool native_drag;
    bool chrome_dragging;
    int drag_window_x;
    int drag_window_y;
    float drag_pointer_x;
    float drag_pointer_y;
};

int l2dcat_preferences_resolved_theme(const L2DCatPreferences *value);
void l2dcat_preferences_apply_theme(L2DCatPreferences *value);

#endif
