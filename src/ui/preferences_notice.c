#include "preferences_notice.h"
#include "preferences_state.h"
#include "ui_backend.h"
#include "ui_catime.h"

#include <SDL3/SDL.h>
#include <stdio.h>

void l2dcat_preferences_notice_show(L2DCatApp *app,
    const char *message, bool error) {
    if (!app || !app->preferences || !message || !message[0]) return;
    L2DCatPreferences *value = app->preferences;
    snprintf(value->notice, sizeof(value->notice), "%s", message);
    value->notice_error = error;
    value->notice_until_ns = SDL_GetTicksNS() + 4000000000ULL;
    l2dcat_preferences_invalidate(value);
}

static void notice_icon(struct nk_command_buffer *canvas,
    struct nk_rect bounds, bool error, struct nk_color color) {
    nk_stroke_circle(canvas, bounds, 2, color);
    float cx = bounds.x + bounds.w * .5f;
    float cy = bounds.y + bounds.h * .5f;
    if (error) {
        nk_stroke_line(canvas, cx - 5, cy - 5, cx + 5, cy + 5, 2, color);
        nk_stroke_line(canvas, cx + 5, cy - 5, cx - 5, cy + 5, 2, color);
    } else {
        nk_stroke_line(canvas, cx - 6, cy, cx - 1, cy + 5, 2, color);
        nk_stroke_line(canvas, cx - 1, cy + 5, cx + 8, cy - 6, 2, color);
    }
}

void l2dcat_preferences_notice_draw(L2DCatPreferences *value,
    struct nk_context *context, float width, float height) {
    if (!value || !value->notice[0]) return;
    uint64_t now = SDL_GetTicksNS();
    if (now >= value->notice_until_ns) {
        value->notice[0] = '\0';
        value->notice_until_ns = 0;
        return;
    }
    value->render_dirty = true;
    bool dark = l2dcat_ui_dark(context);
    L2DCatUIPalette p = l2dcat_ui_palette(dark);
    float toast_width = NK_MIN(390.0f, width - 64.0f);
    struct nk_rect bounds = nk_rect(width - toast_width - 30.0f,
        height - 30.0f - 62.0f, toast_width, 62.0f);
    struct nk_command_buffer *canvas = nk_window_get_canvas(context);
    nk_fill_rect(canvas, nk_rect(bounds.x + 2, bounds.y + 4,
        bounds.w, bounds.h), 13, nk_rgba(0, 0, 0, dark ? 100 : 35));
    nk_fill_rect(canvas, bounds, 13, p.surface);
    struct nk_color tone = value->notice_error ? p.danger : p.accent;
    nk_stroke_rect(canvas, bounds, 13, 1, tone);
    nk_fill_rect(canvas, nk_rect(bounds.x, bounds.y + 10, 4,
        bounds.h - 20), 2, tone);
    notice_icon(canvas, nk_rect(bounds.x + 18, bounds.y + 19, 24, 24),
        value->notice_error, tone);
    const struct nk_user_font *font = l2dcat_ui_body_font(context);
    struct nk_rect text = nk_rect(bounds.x + 54,
        bounds.y + (bounds.h - font->height) * .5f,
        bounds.w - 70, font->height);
    nk_draw_text(canvas, text, value->notice, nk_strlen(value->notice), font,
        nk_rgba(0, 0, 0, 0), p.text);
}
