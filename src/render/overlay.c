#include "l2dcat/overlay.h"
#include "l2dcat/gl_api.h"
#include "l2dcat/image.h"
#include "l2dcat/path.h"

#include <SDL3/SDL_opengl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct TextureSlot {
    char path[L2DCAT_PATH_CAP];
    GLuint texture;
    uint64_t used;
} TextureSlot;

struct L2DCatOverlay {
    L2DCatGL gl;
    GLuint program;
    GLuint clip_program;
    GLuint vao;
    GLuint vbo;
    GLuint background;
    GLuint composite;
    bool composed_cover;
    bool clean_paws;
    bool composite_dirty;
    TextureSlot cache[4];
    GLuint left;
    GLuint right;
    char left_name[L2DCAT_ID_CAP];
    char right_name[L2DCAT_ID_CAP];
    char background_path[L2DCAT_PATH_CAP];
    char left_path[L2DCAT_PATH_CAP];
    char right_path[L2DCAT_PATH_CAP];
    char directory[L2DCAT_PATH_CAP];
    uint64_t clock;
};

static const char *vertex_source =
    "#version 330 core\n"
    "layout(location=0) in vec2 pos; layout(location=1) in vec2 uv;\n"
    "out vec2 tex; uniform bool mirror;\n"
    "void main(){float x=mirror?-pos.x:pos.x;gl_Position=vec4(x,pos.y,0,1);tex=uv;}";
static const char *fragment_source =
    "#version 330 core\n"
    "in vec2 tex; out vec4 color; uniform sampler2D image;\n"
    "uniform bool erase_left;uniform bool erase_right;\n"
    "void main(){color=texture(image,tex);if(color.a<=.001)discard;"
    "vec2 dl=(tex-vec2(.700,.515))/vec2(.080,.170);"
    "vec2 dr=(tex-vec2(.275,.397))/vec2(.070,.160);"
    "bool l=dot(dl,dl)<1.;bool r=dot(dr,dr)<1.;"
    "if((erase_left&&l)||(erase_right&&r))color.rgb=vec3(1);}";
static const char *clip_vertex_source =
    "#version 330 core\n"
    "layout(location=0) in vec2 pos; layout(location=1) in vec2 uv;\n"
    "out vec2 tex; void main(){gl_Position=vec4(pos,0,1);tex=uv;}";
static const char *clip_fragment_source =
    "#version 330 core\n"
    "in vec2 tex; out vec4 color; uniform int radius_percent;\n"
    "void main(){float r=float(radius_percent)/100.0;"
    "vec2 q=abs(tex-vec2(.5))-(vec2(.5)-r);"
    "if(length(max(q,vec2(0)))>r)discard;color=vec4(1);}";

static void clear_textures(L2DCatOverlay *value) {
    if (value->background) glDeleteTextures(1, &value->background);
    if (value->composite) glDeleteTextures(1, &value->composite);
    value->background = 0;
    value->composite = 0;
    value->clean_paws = false;
    value->composed_cover = false;
    for (size_t i = 0; i < 4; ++i) {
        if (value->cache[i].texture) glDeleteTextures(1, &value->cache[i].texture);
        memset(&value->cache[i], 0, sizeof(value->cache[i]));
    }
    value->left = value->right = 0;
    value->left_name[0] = value->right_name[0] = '\0';
    value->left_path[0] = value->right_path[0] = '\0';
}

L2DCatOverlay *l2dcat_overlay_create(L2DCatError *error) {
    L2DCatOverlay *value = calloc(1, sizeof(*value));
    if (!value || !l2dcat_gl_load(&value->gl, error)) {
        free(value);
        return NULL;
    }
    value->program = l2dcat_gl_program(&value->gl, vertex_source, fragment_source, error);
    if (!value->program) { free(value); return NULL; }
    value->clip_program = l2dcat_gl_program(&value->gl,
        clip_vertex_source, clip_fragment_source, error);
    if (!value->clip_program) {
        value->gl.delete_program(value->program);
        free(value);
        return NULL;
    }
    const float vertices[] = {-1, -1, 0, 1, 1, -1, 1, 1, -1, 1, 0, 0, 1, 1, 1, 0};
    value->gl.gen_vertex_arrays(1, &value->vao);
    value->gl.bind_vertex_array(value->vao);
    value->gl.gen_buffers(1, &value->vbo);
    value->gl.bind_buffer(GL_ARRAY_BUFFER, value->vbo);
    value->gl.buffer_data(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    value->gl.enable_attribute(0);
    value->gl.attribute_pointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, NULL);
    value->gl.enable_attribute(1);
    value->gl.attribute_pointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4,
        (const void *)(sizeof(float) * 2));
    return value;
}

void l2dcat_overlay_destroy(L2DCatOverlay *value) {
    if (!value) return;
    clear_textures(value);
    if (value->vbo) value->gl.delete_buffers(1, &value->vbo);
    if (value->vao) value->gl.delete_vertex_arrays(1, &value->vao);
    if (value->clip_program) value->gl.delete_program(value->clip_program);
    if (value->program) value->gl.delete_program(value->program);
    free(value);
}

L2DCatResult l2dcat_overlay_load(L2DCatOverlay *value, const char *directory, L2DCatError *error) {
    if (!value || !directory) return L2DCAT_ERROR_ARGUMENT;
    clear_textures(value);
    snprintf(value->directory, sizeof(value->directory), "%s", directory);
    char path[L2DCAT_PATH_CAP];
    /* Without the licensed Cubism runtime there is no model renderer.  Use the
       model's composed preview so the desktop pet remains visually complete;
       Cubism builds keep the background-only layer behind the animated model. */
#ifdef L2DCAT_HAS_CUBISM
    l2dcat_path_join(path, sizeof(path), directory, "resources/background.png");
#else
    l2dcat_path_join(path, sizeof(path), directory, "resources/cover.png");
    value->composed_cover = true;
    value->clean_paws = true;
#endif
    snprintf(value->background_path, sizeof(value->background_path), "%s", path);
    if (l2dcat_path_is_file(path)) value->background = l2dcat_image_texture(path, NULL, NULL, error);
    return L2DCAT_OK;
}

#ifdef L2DCAT_HAS_CUBISM
static GLuint cached_texture(L2DCatOverlay *value, const char *path) {
    value->clock++;
    TextureSlot *oldest = NULL;
    for (size_t i = 0; i < 4; ++i) {
        TextureSlot *slot = &value->cache[i];
        if (slot->texture && strcmp(slot->path, path) == 0) {
            slot->used = value->clock;
            return slot->texture;
        }
        if (slot->texture &&
            (slot->texture == value->left || slot->texture == value->right)) continue;
        if (!oldest || !slot->texture || slot->used < oldest->used) oldest = slot;
    }
    if (!oldest) return 0;
    if (oldest->texture) glDeleteTextures(1, &oldest->texture);
    L2DCatError ignored = {0};
    oldest->texture = l2dcat_image_texture(path, NULL, NULL, &ignored);
    snprintf(oldest->path, sizeof(oldest->path), "%s", path);
    oldest->used = value->clock;
    return oldest->texture;
}
#endif

static bool key_path(L2DCatOverlay *value, const char *group, const char *name,
    char path[L2DCAT_PATH_CAP]) {
    char relative[L2DCAT_PATH_CAP];
    snprintf(relative, sizeof(relative), "resources/%s/%s.png", group, name);
    l2dcat_path_join(path, L2DCAT_PATH_CAP, value->directory, relative);
    if (l2dcat_path_is_file(path)) return true;
    if (name[0] == 'F' && name[1] >= '0' && name[1] <= '9') {
        snprintf(relative, sizeof(relative), "resources/%s/Fn.png", group);
        l2dcat_path_join(path, L2DCAT_PATH_CAP, value->directory, relative);
        return l2dcat_path_is_file(path);
    }
    return false;
}

int l2dcat_overlay_key(L2DCatOverlay *value, const char *name, bool pressed) {
    if (!value || !name) return -1;
    bool right;
    char path[L2DCAT_PATH_CAP];
    if (key_path(value, "right-keys", name, path)) right = true;
    else if (key_path(value, "left-keys", name, path)) right = false;
    else return -1;
    char *active_name = right ? value->right_name : value->left_name;
    GLuint *active = right ? &value->right : &value->left;
    char *active_path = right ? value->right_path : value->left_path;
    if (pressed) {
        snprintf(active_name, L2DCAT_ID_CAP, "%s", name);
#ifdef L2DCAT_HAS_CUBISM
        *active = cached_texture(value, path);
#else
        snprintf(active_path, L2DCAT_PATH_CAP, "%s", path);
        *active = 1;
#endif
    } else if (strcmp(active_name, name) == 0) {
        active_name[0] = '\0';
        *active = 0;
        active_path[0] = '\0';
    }
#ifndef L2DCAT_HAS_CUBISM
    value->composite_dirty = true;
#endif
    return right ? 1 : 0;
}

bool l2dcat_overlay_hand_active(const L2DCatOverlay *value, bool right) {
    return value && (right ? value->right : value->left) != 0;
}

void l2dcat_overlay_begin_clip(L2DCatOverlay *value, float radius_percent) {
    if (!value || radius_percent <= 0.0f) {
        glDisable(GL_STENCIL_TEST);
        return;
    }
    int radius = (int)(radius_percent + 0.5f);
    if (radius > 50) radius = 50;
    glEnable(GL_STENCIL_TEST);
    glStencilMask(0xff);
    glClear(GL_STENCIL_BUFFER_BIT);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    glStencilFunc(GL_ALWAYS, 1, 0xff);
    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
    value->gl.use_program(value->clip_program);
    value->gl.uniform_1i(value->gl.uniform_location(value->clip_program,
        "radius_percent"), radius);
    value->gl.bind_vertex_array(value->vao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glStencilMask(0x00);
    glStencilFunc(GL_EQUAL, 1, 0xff);
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
}

void l2dcat_overlay_end_clip(L2DCatOverlay *value) {
    (void)value;
    glStencilMask(0xff);
    glDisable(GL_STENCIL_TEST);
}

static void draw(L2DCatOverlay *value, GLuint texture, bool mirror, bool blend) {
    if (!value || !texture) return;
    if (blend) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    } else glDisable(GL_BLEND);
    value->gl.use_program(value->program);
    value->gl.uniform_1i(value->gl.uniform_location(value->program, "mirror"), mirror);
    value->gl.uniform_1i(value->gl.uniform_location(value->program, "image"), 0);
    value->gl.uniform_1i(value->gl.uniform_location(value->program, "erase_left"),
        !blend && value->composed_cover && !value->composite && value->left != 0);
    value->gl.uniform_1i(value->gl.uniform_location(value->program, "erase_right"),
        !blend && value->composed_cover && !value->composite && value->right != 0);
    value->gl.active_texture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    value->gl.bind_vertex_array(value->vao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

void l2dcat_overlay_draw_background(L2DCatOverlay *value, bool mirror) {
    if (value) {
#ifndef L2DCAT_HAS_CUBISM
        if (value->composite_dirty) {
            value->composite_dirty = false;
            if (value->left || value->right) {
                L2DCatError ignored = {0};
                value->composite = l2dcat_image_composite_texture(value->background_path,
                    value->left_path, value->right_path, value->composite,
                    value->clean_paws && value->left,
                    value->clean_paws && value->right, &ignored);
            }
        }
#endif
        bool active = value->left || value->right;
        draw(value, active && value->composite ? value->composite : value->background,
            mirror, false);
    }
}

void l2dcat_overlay_draw_keys(L2DCatOverlay *value, bool mirror) {
    if (!value) return;
#ifdef L2DCAT_HAS_CUBISM
    draw(value, value->left, mirror, true);
    draw(value, value->right, mirror, true);
#else
    (void)mirror;
#endif
}
