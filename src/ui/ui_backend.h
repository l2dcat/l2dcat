#ifndef L2DCAT_UI_BACKEND_H
#define L2DCAT_UI_BACKEND_H

#include "l2dcat/gl_api.h"
#include <SDL3/SDL.h>
#include "nuklear_config.h"

typedef enum L2DCatUICursor {
    L2DCAT_UI_CURSOR_DEFAULT,
    L2DCAT_UI_CURSOR_POINTER,
    L2DCAT_UI_CURSOR_TEXT
} L2DCatUICursor;

typedef struct L2DCatUIBackend {
    SDL_Window *window;
    L2DCatGL gl;
    struct nk_context context;
    struct nk_font_atlas atlas;
    struct nk_buffer commands;
    struct nk_draw_null_texture null_texture;
    GLuint program;
    GLint texture_location;
    GLint projection_location;
    GLuint vao;
    GLuint vbo;
    GLuint ebo;
    GLuint font_texture;
    void *font_blob;
    size_t font_blob_size;
    void *font_file_handle;
    void *font_mapping_handle;
    void *vertices;
    void *elements;
    size_t vertex_capacity;
    size_t element_capacity;
    size_t last_vertex_bytes;
    size_t last_element_bytes;
    unsigned last_draw_commands;
    unsigned last_draw_elements;
    int last_convert_result;
    GLenum last_gl_error;
    unsigned nonzero_alpha_vertices;
    unsigned max_alpha;
    bool custom_font_loaded;
    bool font_probe_loaded;
    bool font_path_found;
    bool font_file_loaded;
    SDL_Cursor *pointer_cursor;
    SDL_Cursor *text_cursor;
    L2DCatUICursor requested_cursor;
} L2DCatUIBackend;

bool l2dcat_ui_init(L2DCatUIBackend *ui, SDL_Window *window, const char *font_path,
    const nk_rune *glyph_ranges, L2DCatError *error);
void l2dcat_ui_destroy(L2DCatUIBackend *ui);
void l2dcat_ui_input_begin(L2DCatUIBackend *ui);
void l2dcat_ui_input_end(L2DCatUIBackend *ui);
bool l2dcat_ui_event(L2DCatUIBackend *ui, const SDL_Event *event);
void l2dcat_ui_render(L2DCatUIBackend *ui);
bool l2dcat_ui_frame_valid(const L2DCatUIBackend *ui);
void l2dcat_ui_cursor_begin(L2DCatUIBackend *ui);
void l2dcat_ui_cursor_apply(L2DCatUIBackend *ui);
void l2dcat_ui_cursor_destroy(L2DCatUIBackend *ui);
void l2dcat_ui_cursor_reset(struct nk_context *context);
void l2dcat_ui_cursor_hover_rect(struct nk_context *context,
    struct nk_rect bounds, L2DCatUICursor cursor);
void l2dcat_ui_cursor_hover_widget(struct nk_context *context,
    L2DCatUICursor cursor);

#endif
