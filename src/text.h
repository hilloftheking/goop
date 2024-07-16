#pragma once

#include "HandmadeMath.h"

#include "stb/stb_truetype.h"

typedef struct TextRenderer {
  float font_height;
  void *ttf_data;
  stbtt_bakedchar *cdata;
  unsigned int font_tex, glyph_program;
} TextRenderer;

typedef struct TextBox {
  HMM_Vec2 pos;
  const char *text;
} TextBox;

void text_renderer_create(TextRenderer *tr, const char *font_path,
                          float font_height);
void text_renderer_destroy(TextRenderer *tr);

void text_render(TextRenderer *tr, const char *text, float x, float y);