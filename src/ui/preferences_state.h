#ifndef BONGO_CAT_NEO_PREFERENCES_STATE_H
#define BONGO_CAT_NEO_PREFERENCES_STATE_H

#include "preferences_internal.h"
#include "ui_backend.h"
#include "bongo_cat_neo/i18n.h"
#include "bongo_cat_neo/preferences.h"

struct BongoCatNeoPreferences {
    BongoCatNeoApp *app;
    SDL_Window *window;
    SDL_GLContext gl_context;
    bool owns_gl_context;
    BongoCatNeoUIBackend ui;
    int page;
    int style_theme;
    BongoCatNeoLanguage font_language;
    nk_rune glyph_ranges[2048];
    bool input_active;
    bool import_requested;
    BongoCatNeoImportDialog *import_dialog;
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

int bongo_cat_neo_preferences_resolved_theme(const BongoCatNeoPreferences *value);
void bongo_cat_neo_preferences_apply_theme(BongoCatNeoPreferences *value);

#endif
