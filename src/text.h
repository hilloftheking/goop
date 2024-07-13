#pragma once

#include "stb/stb_truetype.h"

typedef struct TextRenderer {
  float font_height;
  void *ttf_data;
  stbtt_bakedchar *cdata;
  unsigned int font_tex, vao, vbo, ibo, glyph_program;
} TextRenderer;

void text_renderer_create(TextRenderer *tr, const char *font_path,
                          float font_height);
void text_renderer_destroy(TextRenderer *tr);

void text_render(TextRenderer *tr, const char *text, float x, float y);