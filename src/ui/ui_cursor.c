#include "ui_backend.h"

static L2DCatUIBackend *active_backend;

static void apply_requested(L2DCatUIBackend *ui) {
    SDL_Cursor *cursor = SDL_GetDefaultCursor();
    if (ui->requested_cursor == L2DCAT_UI_CURSOR_POINTER && ui->pointer_cursor)
        cursor = ui->pointer_cursor;
    else if (ui->requested_cursor == L2DCAT_UI_CURSOR_TEXT && ui->text_cursor)
        cursor = ui->text_cursor;
    if (!cursor) return;
    if (SDL_GetCursor() != cursor) SDL_SetCursor(cursor);
    else SDL_SetCursor(NULL);
}

static L2DCatUIBackend *backend_for(const struct nk_context *context) {
    return active_backend && context == &active_backend->context
        ? active_backend : NULL;
}

void l2dcat_ui_cursor_begin(L2DCatUIBackend *ui) {
    if (!ui) return;
    active_backend = ui;
    if (!ui->pointer_cursor)
        ui->pointer_cursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_POINTER);
    if (!ui->text_cursor)
        ui->text_cursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_TEXT);
    ui->requested_cursor = L2DCAT_UI_CURSOR_DEFAULT;
    apply_requested(ui);
}

void l2dcat_ui_cursor_reset(struct nk_context *context) {
    L2DCatUIBackend *ui = backend_for(context);
    if (ui) { ui->requested_cursor = L2DCAT_UI_CURSOR_DEFAULT; apply_requested(ui); }
}

static void request_cursor(struct nk_context *context, L2DCatUICursor cursor) {
    L2DCatUIBackend *ui = backend_for(context);
    if (!ui) return;
    if (cursor == L2DCAT_UI_CURSOR_TEXT &&
        ui->requested_cursor == L2DCAT_UI_CURSOR_POINTER) return;
    ui->requested_cursor = cursor;
    apply_requested(ui);
}

void l2dcat_ui_cursor_hover_rect(struct nk_context *context,
    struct nk_rect bounds, L2DCatUICursor cursor) {
    if (context && nk_input_is_mouse_hovering_rect(&context->input, bounds))
        request_cursor(context, cursor);
}

void l2dcat_ui_cursor_hover_widget(struct nk_context *context,
    L2DCatUICursor cursor) {
    if (context && nk_widget_is_hovered(context)) request_cursor(context, cursor);
}

void l2dcat_ui_cursor_apply(L2DCatUIBackend *ui) {
    if (ui) apply_requested(ui);
}

void l2dcat_ui_cursor_destroy(L2DCatUIBackend *ui) {
    if (!ui) return;
    SDL_Cursor *current = SDL_GetCursor();
    if (current == ui->pointer_cursor || current == ui->text_cursor)
        SDL_SetCursor(SDL_GetDefaultCursor());
    if (ui->pointer_cursor) SDL_DestroyCursor(ui->pointer_cursor);
    if (ui->text_cursor) SDL_DestroyCursor(ui->text_cursor);
    ui->pointer_cursor = NULL;
    ui->text_cursor = NULL;
    if (active_backend == ui) active_backend = NULL;
}
