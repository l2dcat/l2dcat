#include "ui_font_atlas.h"
#include "bongo_cat_neo/file.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <io.h>
#include <windows.h>
#endif

typedef struct UIFontSource {
    void *data;
    size_t size;
    FILE *file;
    void *mapping;
} UIFontSource;

static bool source_load(UIFontSource *source, const char *path) {
    source->file = bongo_cat_neo_file_open(path, "rb");
    if (!source->file || fseek(source->file, 0, SEEK_END) != 0) return false;
    long size = ftell(source->file);
    if (size <= 0 || fseek(source->file, 0, SEEK_SET) != 0) return false;
#ifdef _WIN32
    HANDLE file = (HANDLE)_get_osfhandle(_fileno(source->file));
    HANDLE mapping = CreateFileMappingW(file, NULL, PAGE_READONLY, 0, 0, NULL);
    source->data = mapping ? MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0) : NULL;
    source->mapping = mapping;
    source->size = source->data ? (size_t)size : 0;
#else
    source->data = malloc((size_t)size);
    source->size = source->data
        ? fread(source->data, 1, (size_t)size, source->file) : 0;
    if (source->size != (size_t)size) {
        free(source->data);
        source->data = NULL;
        source->size = 0;
    }
#endif
    return source->data != NULL;
}

static void source_release(UIFontSource *source) {
#ifdef _WIN32
    if (source->data) UnmapViewOfFile(source->data);
    if (source->mapping) CloseHandle((HANDLE)source->mapping);
#else
    free(source->data);
#endif
    if (source->file) fclose(source->file);
    memset(source, 0, sizeof(*source));
}

static struct nk_font_config font_config(float size, const nk_rune *ranges) {
    struct nk_font_config config = nk_font_config(size);
    config.range = ranges;
    config.oversample_h = 1;
    config.oversample_v = 1;
    return config;
}

static struct nk_font *add_font(struct nk_font_atlas *atlas,
    const UIFontSource *source, float size, const nk_rune *ranges) {
    struct nk_font_config config = font_config(size, ranges);
    return source && source->data
        ? nk_font_atlas_add_from_memory(atlas, source->data,
            source->size, size, &config)
        : nk_font_atlas_add_default(atlas, size, &config);
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

static bool upload_atlas(BongoCatNeoUIBackend *ui) {
    int width = 0, height = 0;
    const void *pixels = nk_font_atlas_bake(&ui->atlas, &width, &height,
        NK_FONT_ATLAS_ALPHA8);
    if (!pixels || width < 1 || height < 1) return false;
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
    nk_font_atlas_end(&ui->atlas, nk_handle_id((int)ui->font_texture),
        &ui->null_texture);
    return ui->font_texture != 0;
}

bool bongo_cat_neo_ui_font_atlas_create(BongoCatNeoUIBackend *ui,
    const char *body_path, const char *heading_path,
    const nk_rune *glyph_ranges) {
    UIFontSource body = {0}, heading = {0};
    bool body_loaded = body_path && source_load(&body, body_path);
    bool same_path = body_path && heading_path &&
        strcmp(body_path, heading_path) == 0;
    bool heading_loaded = same_path ? body_loaded :
        (heading_path && source_load(&heading, heading_path));
    const UIFontSource *heading_source = same_path ? &body :
        (heading_loaded ? &heading : (body_loaded ? &body : NULL));
    nk_font_atlas_init_default(&ui->atlas);
    nk_font_atlas_begin(&ui->atlas);
    struct nk_font *caption_font = add_font(&ui->atlas,
        body_loaded ? &body : NULL, 15.0f, glyph_ranges);
    struct nk_font *body_font = add_font(&ui->atlas,
        body_loaded ? &body : NULL, 18.0f, glyph_ranges);
    struct nk_font *label_font = add_font(&ui->atlas,
        heading_source, 17.0f, glyph_ranges);
    struct nk_font *heading_font = add_font(&ui->atlas,
        heading_source, 24.0f, glyph_ranges);
    bool uploaded = caption_font && body_font && label_font && heading_font &&
        upload_atlas(ui);
    if (uploaded) {
        ui->caption_font = &caption_font->handle;
        ui->body_font = &body_font->handle;
        ui->label_font = &label_font->handle;
        ui->heading_font = &heading_font->handle;
        ui->font_probe_loaded = font_has_ranges(body_font, glyph_ranges);
        nk_style_set_font(&ui->context, ui->body_font);
    }
    ui->font_path_found = body_path != NULL;
    ui->font_file_loaded = body_loaded;
    ui->custom_font_loaded = body_loaded && body_font != NULL;
    nk_font_atlas_cleanup(&ui->atlas);
    source_release(&heading);
    source_release(&body);
    return uploaded;
}

void bongo_cat_neo_ui_font_atlas_destroy(BongoCatNeoUIBackend *ui) {
    if (!ui) return;
    nk_font_atlas_clear(&ui->atlas);
    ui->caption_font = NULL;
    ui->body_font = NULL;
    ui->label_font = NULL;
    ui->heading_font = NULL;
}
