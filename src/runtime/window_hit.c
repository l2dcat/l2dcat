#include "runtime.h"

#include <SDL3/SDL_opengl.h>

bool l2dcat_window_visible_at_pointer(L2DCatApp *app, float x, float y) {
    int width, height, pixel_width, pixel_height;
    if (!SDL_GetWindowSize(app->window, &width, &height) ||
        !SDL_GetWindowSizeInPixels(app->window, &pixel_width, &pixel_height) ||
        width <= 0 || height <= 0 || pixel_width <= 0 || pixel_height <= 0) return false;
    int pixel_x = SDL_clamp((int)(x * pixel_width / width), 0, pixel_width - 1);
    int pixel_y = pixel_height - 1 -
        SDL_clamp((int)(y * pixel_height / height), 0, pixel_height - 1);
    SDL_Window *previous_window = SDL_GL_GetCurrentWindow();
    SDL_GLContext previous_context = SDL_GL_GetCurrentContext();
    if (!SDL_GL_MakeCurrent(app->window, app->gl_context)) return false;
    GLint previous_buffer;
    GLubyte pixel[4] = {0};
    glGetIntegerv(GL_READ_BUFFER, &previous_buffer);
    glReadBuffer(GL_FRONT);
    glReadPixels(pixel_x, pixel_y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    if (pixel[3] <= 8) {
        glReadBuffer(GL_BACK);
        glReadPixels(pixel_x, pixel_y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    }
    glReadBuffer((GLenum)previous_buffer);
    if (previous_window && previous_context)
        SDL_GL_MakeCurrent(previous_window, previous_context);
    return pixel[3] > 8;
}

void l2dcat_window_apply_pending_resize(L2DCatApp *app) {
    if (!app || !app->resize_pending) return;
    app->resize_pending = false;
    l2dcat_live2d_resize(app->live2d,
        app->resize_pixel_width, app->resize_pixel_height);
}
