#include "ui_backend.h"
#include "l2dcat/file.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <io.h>
#include <windows.h>
#endif

typedef struct UIVertex {
    float position[2];
    float uv[2];
    nk_byte color[4];
} UIVertex;

static const char *vertex_source =
    "#version 330 core\n"
    "layout(location=0) in vec2 Position;layout(location=1) in vec2 TexCoord;"
    "layout(location=2) in vec4 Color;uniform mat4 Projection;"
    "out vec2 FragUV;out vec4 FragColor;"
    "void main(){FragUV=TexCoord;FragColor=Color;gl_Position=Projection*vec4(Position,0,1);}";
static const char *fragment_source =
    "#version 330 core\n"
    "in vec2 FragUV;in vec4 FragColor;uniform sampler2D Texture;out vec4 OutColor;"
    "void main(){OutColor=FragColor*texture(Texture,FragUV);}";

static void clipboard_copy(nk_handle user, const char *text, int length) {
    (void)user;
    if (!text || length <= 0) return;
    char *copy = malloc((size_t)length + 1);
    if (!copy) return;
    memcpy(copy, text, (size_t)length);
    copy[length] = '\0';
    SDL_SetClipboardText(copy);
    free(copy);
}

static void clipboard_paste(nk_handle user, struct nk_text_edit *edit) {
    (void)user;
    char *text = SDL_GetClipboardText();
    if (text) {
        nk_textedit_paste(edit, text, nk_strlen(text));
        SDL_free(text);
    }
}

static bool load_font_file(L2DCatUIBackend *ui, const char *path) {
    FILE *file = l2dcat_file_open(path, "rb");
    if (!file || fseek(file, 0, SEEK_END) != 0) {
        if (file) fclose(file);
        return false;
    }
    long size = ftell(file);
    if (size <= 0 || fseek(file, 0, SEEK_SET) != 0) { fclose(file); return false; }
#ifdef _WIN32
    HANDLE source = (HANDLE)_get_osfhandle(_fileno(file));
    HANDLE mapping = CreateFileMappingW(source, NULL, PAGE_READONLY, 0, 0, NULL);
    void *view = mapping ? MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0) : NULL;
    if (!view) {
        if (mapping) CloseHandle(mapping);
        fclose(file);
        return false;
    }
    ui->font_blob = view;
    ui->font_blob_size = (size_t)size;
    ui->font_file_handle = file;
    ui->font_mapping_handle = mapping;
    return true;
#else
    ui->font_blob = malloc((size_t)size);
    if (!ui->font_blob) { fclose(file); return false; }
    ui->font_blob_size = fread(ui->font_blob, 1, (size_t)size, file);
    fclose(file);
    if (ui->font_blob_size == (size_t)size) return true;
    free(ui->font_blob); ui->font_blob = NULL; ui->font_blob_size = 0;
    return false;
#endif
}

static void release_font_file(L2DCatUIBackend *ui) {
#ifdef _WIN32
    if (ui->font_blob) UnmapViewOfFile(ui->font_blob);
    if (ui->font_mapping_handle) CloseHandle(ui->font_mapping_handle);
    if (ui->font_file_handle) fclose(ui->font_file_handle);
#else
    free(ui->font_blob);
#endif
    ui->font_blob = NULL; ui->font_blob_size = 0;
    ui->font_file_handle = NULL; ui->font_mapping_handle = NULL;
}

static bool font_has_ranges(const struct nk_font *font, const nk_rune *ranges) {
    if (!font) return false;
    if (!ranges) {
        const struct nk_font_glyph *glyph = nk_font_find_glyph(font, 'A');
        return glyph && glyph->codepoint == 'A';
    }
    for (size_t pair = 2; ranges[pair] && ranges[pair + 1]; pair += 2)
        for (nk_rune point = ranges[pair]; point <= ranges[pair + 1]; ++point) {
            const struct nk_font_glyph *glyph = nk_font_find_glyph(font, point);
            if (!glyph || glyph->codepoint != point) return false;
        }
    return true;
}

static bool create_device(L2DCatUIBackend *ui, L2DCatError *error) {
    if (!l2dcat_gl_load(&ui->gl, error)) return false;
    ui->program = l2dcat_gl_program(&ui->gl, vertex_source, fragment_source, error);
    if (!ui->program) return false;
    ui->texture_location = ui->gl.uniform_location(ui->program, "Texture");
    ui->projection_location = ui->gl.uniform_location(ui->program, "Projection");
    ui->gl.gen_vertex_arrays(1, &ui->vao);
    ui->gl.bind_vertex_array(ui->vao);
    ui->gl.gen_buffers(1, &ui->vbo);
    ui->gl.gen_buffers(1, &ui->ebo);
    ui->gl.bind_buffer(GL_ARRAY_BUFFER, ui->vbo);
    ui->gl.bind_buffer(GL_ELEMENT_ARRAY_BUFFER, ui->ebo);
    ui->gl.enable_attribute(0);
    ui->gl.attribute_pointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(UIVertex),
        (void *)offsetof(UIVertex, position));
    ui->gl.enable_attribute(1);
    ui->gl.attribute_pointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(UIVertex),
        (void *)offsetof(UIVertex, uv));
    ui->gl.enable_attribute(2);
    ui->gl.attribute_pointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(UIVertex),
        (void *)offsetof(UIVertex, color));
    return true;
}

static bool create_font(L2DCatUIBackend *ui, const char *font_path,
    const nk_rune *glyph_ranges) {
    nk_font_atlas_init_default(&ui->atlas);
    nk_font_atlas_begin(&ui->atlas);
    struct nk_font_config config = nk_font_config(17.0f);
    config.range = glyph_ranges;
    config.oversample_h = 1;
    config.oversample_v = 1;
    bool loaded = font_path && load_font_file(ui, font_path);
    ui->font_path_found = font_path != NULL;
    ui->font_file_loaded = loaded;
    struct nk_font *font = loaded
        ? nk_font_atlas_add_from_memory(&ui->atlas, ui->font_blob,
            ui->font_blob_size, 17.0f, &config)
        : nk_font_atlas_add_default(&ui->atlas, 17.0f, NULL);
    ui->custom_font_loaded = loaded && font;
    if (!font && loaded) {
        release_font_file(ui);
        font = nk_font_atlas_add_default(&ui->atlas, 17.0f, NULL);
    }
    int width, height;
    const void *pixels = nk_font_atlas_bake(&ui->atlas, &width, &height, NK_FONT_ATLAS_ALPHA8);
    glGenTextures(1, &ui->font_texture);
    glBindTexture(GL_TEXTURE_2D, ui->font_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_ONE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_ONE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_ONE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_RED);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, width, height, 0, GL_RED,
        GL_UNSIGNED_BYTE, pixels);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    nk_font_atlas_end(&ui->atlas, nk_handle_id((int)ui->font_texture), &ui->null_texture);
    if (font) {
        ui->font_probe_loaded = font_has_ranges(font, glyph_ranges);
        nk_style_set_font(&ui->context, &font->handle);
    }
    nk_font_atlas_cleanup(&ui->atlas);
    release_font_file(ui);
    return font && ui->font_texture != 0;
}

bool l2dcat_ui_init(L2DCatUIBackend *ui, SDL_Window *window, const char *font_path,
    const nk_rune *glyph_ranges, L2DCatError *error) {
    memset(ui, 0, sizeof(*ui));
    ui->window = window;
    ui->vertex_capacity = 512 * 1024;
    ui->element_capacity = 128 * 1024;
    ui->vertices = malloc(ui->vertex_capacity);
    ui->elements = malloc(ui->element_capacity);
    if (!ui->vertices || !ui->elements || !nk_init_default(&ui->context, NULL) ||
        !create_device(ui, error) || !create_font(ui, font_path, glyph_ranges)) return false;
    nk_buffer_init_default(&ui->commands);
    ui->context.clip.copy = clipboard_copy;
    ui->context.clip.paste = clipboard_paste;
    return true;
}

void l2dcat_ui_destroy(L2DCatUIBackend *ui) {
    if (!ui) return;
    nk_font_atlas_clear(&ui->atlas);
    release_font_file(ui);
    nk_buffer_free(&ui->commands);
    nk_free(&ui->context);
    if (ui->font_texture) glDeleteTextures(1, &ui->font_texture);
    if (ui->vbo) ui->gl.delete_buffers(1, &ui->vbo);
    if (ui->ebo) ui->gl.delete_buffers(1, &ui->ebo);
    if (ui->vao) ui->gl.delete_vertex_arrays(1, &ui->vao);
    if (ui->program) ui->gl.delete_program(ui->program);
    free(ui->vertices);
    free(ui->elements);
    memset(ui, 0, sizeof(*ui));
}

static void convert(L2DCatUIBackend *ui) {
    static const struct nk_draw_vertex_layout_element layout[] = {
        {NK_VERTEX_POSITION, NK_FORMAT_FLOAT, offsetof(UIVertex, position)},
        {NK_VERTEX_TEXCOORD, NK_FORMAT_FLOAT, offsetof(UIVertex, uv)},
        {NK_VERTEX_COLOR, NK_FORMAT_R8G8B8A8, offsetof(UIVertex, color)},
        {NK_VERTEX_LAYOUT_END}
    };
    struct nk_convert_config config = {0};
    config.vertex_layout = layout;
    config.vertex_size = sizeof(UIVertex);
    config.vertex_alignment = NK_ALIGNOF(UIVertex);
    config.tex_null = ui->null_texture;
    config.global_alpha = 1.0f;
    config.circle_segment_count = 18;
    config.curve_segment_count = 18;
    config.arc_segment_count = 18;
    config.shape_AA = NK_ANTI_ALIASING_ON;
    config.line_AA = NK_ANTI_ALIASING_ON;
    struct nk_buffer vertices, elements;
    nk_buffer_init_fixed(&vertices, ui->vertices, ui->vertex_capacity);
    nk_buffer_init_fixed(&elements, ui->elements, ui->element_capacity);
    ui->last_convert_result = nk_convert(&ui->context,
        &ui->commands, &vertices, &elements, &config);
    ui->last_vertex_bytes = vertices.allocated;
    ui->last_element_bytes = elements.allocated;
    if (vertices.allocated >= sizeof(UIVertex)) {
        const UIVertex *sample = ui->vertices;
        size_t count = vertices.allocated / sizeof(*sample);
        ui->nonzero_alpha_vertices = 0; ui->max_alpha = 0;
        for (size_t i = 0; i < count; ++i) {
            if (sample[i].color[3]) ui->nonzero_alpha_vertices++;
            if (sample[i].color[3] > ui->max_alpha) ui->max_alpha = sample[i].color[3];
        }
    }
    ui->gl.bind_buffer(GL_ARRAY_BUFFER, ui->vbo);
    ui->gl.buffer_data(GL_ARRAY_BUFFER, (GLsizeiptr)vertices.allocated,
        ui->vertices, GL_STREAM_DRAW);
    ui->gl.bind_buffer(GL_ELEMENT_ARRAY_BUFFER, ui->ebo);
    ui->gl.buffer_data(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)elements.allocated,
        ui->elements, GL_STREAM_DRAW);
}

void l2dcat_ui_render(L2DCatUIBackend *ui) {
    int width, height, pixel_width, pixel_height;
    SDL_GetWindowSize(ui->window, &width, &height);
    SDL_GetWindowSizeInPixels(ui->window, &pixel_width, &pixel_height);
    float projection[4][4] = {{2.0f / width, 0, 0, 0}, {0, -2.0f / height, 0, 0},
        {0, 0, -1, 0}, {-1, 1, 0, 1}};
    while (glGetError() != GL_NO_ERROR) {}
    convert(ui);
    glViewport(0, 0, pixel_width, pixel_height);
    glEnable(GL_BLEND);
    ui->gl.blend_equation(GL_FUNC_ADD);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_SCISSOR_TEST);
    ui->gl.use_program(ui->program);
    ui->gl.uniform_1i(ui->texture_location, 0);
    ui->gl.uniform_matrix_4fv(ui->projection_location, 1, GL_FALSE, &projection[0][0]);
    ui->gl.active_texture(GL_TEXTURE0);
    ui->gl.bind_vertex_array(ui->vao);
    const struct nk_draw_command *command;
    const nk_draw_index *offset = NULL;
    float sx = (float)pixel_width / width, sy = (float)pixel_height / height;
    ui->last_draw_commands = 0;
    ui->last_draw_elements = 0;
    nk_draw_foreach(command, &ui->context, &ui->commands) {
        if (!command->elem_count) continue;
        ui->last_draw_commands++;
        ui->last_draw_elements += command->elem_count;
        glBindTexture(GL_TEXTURE_2D, (GLuint)command->texture.id);
        glScissor((GLint)(command->clip_rect.x * sx),
            (GLint)((height - command->clip_rect.y - command->clip_rect.h) * sy),
            (GLsizei)(command->clip_rect.w * sx), (GLsizei)(command->clip_rect.h * sy));
        glDrawElements(GL_TRIANGLES, (GLsizei)command->elem_count,
            sizeof(nk_draw_index) == 2 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT, offset);
        offset += command->elem_count;
    }
    nk_clear(&ui->context);
    nk_buffer_clear(&ui->commands);
    glDisable(GL_SCISSOR_TEST);
    ui->last_gl_error = glGetError();
}

bool l2dcat_ui_frame_valid(const L2DCatUIBackend *ui) {
    return ui && ui->last_convert_result == NK_CONVERT_SUCCESS &&
        ui->last_vertex_bytes && ui->last_element_bytes &&
        ui->last_draw_commands && ui->last_draw_elements &&
        ui->nonzero_alpha_vertices && ui->max_alpha &&
        ui->font_probe_loaded &&
        ui->last_gl_error == GL_NO_ERROR;
}
