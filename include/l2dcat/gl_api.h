#ifndef L2DCAT_GL_API_H
#define L2DCAT_GL_API_H

#include "l2dcat/common.h"
#include <SDL3/SDL_opengl.h>

typedef struct L2DCatGL {
    PFNGLCREATESHADERPROC create_shader;
    PFNGLSHADERSOURCEPROC shader_source;
    PFNGLCOMPILESHADERPROC compile_shader;
    PFNGLGETSHADERIVPROC get_shader_iv;
    PFNGLGETSHADERINFOLOGPROC get_shader_log;
    PFNGLCREATEPROGRAMPROC create_program;
    PFNGLATTACHSHADERPROC attach_shader;
    PFNGLLINKPROGRAMPROC link_program;
    PFNGLGETPROGRAMIVPROC get_program_iv;
    PFNGLGETPROGRAMINFOLOGPROC get_program_log;
    PFNGLDELETESHADERPROC delete_shader;
    PFNGLDELETEPROGRAMPROC delete_program;
    PFNGLUSEPROGRAMPROC use_program;
    PFNGLGENVERTEXARRAYSPROC gen_vertex_arrays;
    PFNGLBINDVERTEXARRAYPROC bind_vertex_array;
    PFNGLDELETEVERTEXARRAYSPROC delete_vertex_arrays;
    PFNGLGENBUFFERSPROC gen_buffers;
    PFNGLBINDBUFFERPROC bind_buffer;
    PFNGLBUFFERDATAPROC buffer_data;
    PFNGLDELETEBUFFERSPROC delete_buffers;
    PFNGLENABLEVERTEXATTRIBARRAYPROC enable_attribute;
    PFNGLVERTEXATTRIBPOINTERPROC attribute_pointer;
    PFNGLGETUNIFORMLOCATIONPROC uniform_location;
    PFNGLUNIFORM1IPROC uniform_1i;
    PFNGLUNIFORMMATRIX4FVPROC uniform_matrix_4fv;
    PFNGLACTIVETEXTUREPROC active_texture;
    PFNGLBLENDEQUATIONPROC blend_equation;
} L2DCatGL;

bool l2dcat_gl_load(L2DCatGL *gl, L2DCatError *error);
unsigned int l2dcat_gl_program(L2DCatGL *gl, const char *vertex, const char *fragment,
    L2DCatError *error);

#endif
