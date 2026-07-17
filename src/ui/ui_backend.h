#ifndef BONGO_UI_BACKEND_H
#define BONGO_UI_BACKEND_H

#include "bongo/gl_api.h"
#include <SDL3/SDL.h>
#include "nuklear_config.h"

typedef struct BongoUIBackend {
    SDL_Window *window;
    BongoGL gl;
    struct nk_context context;
    struct nk_font_atlas atlas;
    struct nk_buffer commands;
    struct nk_draw_null_texture null_texture;
    GLuint program;
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
} BongoUIBackend;

bool bongo_ui_init(BongoUIBackend *ui, SDL_Window *window, const char *font_path,
    const nk_rune *glyph_ranges, BongoError *error);
void bongo_ui_destroy(BongoUIBackend *ui);
void bongo_ui_input_begin(BongoUIBackend *ui);
void bongo_ui_input_end(BongoUIBackend *ui);
bool bongo_ui_event(BongoUIBackend *ui, const SDL_Event *event);
void bongo_ui_render(BongoUIBackend *ui);
bool bongo_ui_frame_valid(const BongoUIBackend *ui);

#endif
