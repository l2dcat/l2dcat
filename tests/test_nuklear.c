#include "../src/ui/nuklear_config.h"

#include <stdio.h>

int main(void) {
    struct nk_context context;
    struct nk_font_atlas atlas;
    struct nk_draw_null_texture null_texture;
    if (!nk_init_default(&context, NULL)) return 1;
    nk_font_atlas_init_default(&atlas);
    nk_font_atlas_begin(&atlas);
    struct nk_font *font = nk_font_atlas_add_default(&atlas, 16.0f, NULL);
    if (!font) return 2;
    int width, height;
    if (!nk_font_atlas_bake(&atlas, &width, &height, NK_FONT_ATLAS_RGBA32)) return 3;
    nk_font_atlas_end(&atlas, nk_handle_id(1), &null_texture);
    nk_style_set_font(&context, &font->handle);
    if (!nk_begin(&context, "test", nk_rect(0, 0, 320, 240), 0)) return 4;
    nk_layout_row_dynamic(&context, 24, 1);
    nk_label(&context, "Nuklear", NK_TEXT_LEFT);
    nk_end(&context);
    nk_clear(&context);
    nk_free(&context);
    nk_font_atlas_clear(&atlas);
    puts("nuklear smoke passed");
    return 0;
}
