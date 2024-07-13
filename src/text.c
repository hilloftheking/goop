#include <stdint.h>
#include <stdio.h>

#include <glad/glad.h>

#include "HandmadeMath.h"

#include "core.h"
#include "shader.h"
#include "shader_sources.h"
#include "text.h"

#define FONT_BITMAP_SIZE 512
#define FONT_CHAR_START 32
#define FONT_CHAR_COUNT 96

static const HMM_Vec2 QUAD_VERTS[] = {
    {-0.5f, -0.5f}, {0.5f, -0.5f}, {0.5f, 0.5f}, {-0.5f, 0.5f}};
static const uint8_t QUAD_INDICES[] = {0, 1, 2, 2, 3, 0};

void text_renderer_create(TextRenderer *tr, const char *font_path,
                          float font_height) {
  tr->font_height = font_height;

  FILE *f = fopen(font_path, "rb");
  if (!f) {
    fprintf(stderr, "Failed to open font %s\n", font_path);
    exit_fatal_error();
    return;
  }

  fseek(f, 0, SEEK_END);
  int fsize = ftell(f);
  tr->ttf_data = alloc_mem(fsize);
  fseek(f, 0, SEEK_SET);
  fread(tr->ttf_data, 1, fsize, f);
  fclose(f);

  unsigned char *font_pixels = alloc_mem(FONT_BITMAP_SIZE * FONT_BITMAP_SIZE);
  tr->cdata = alloc_mem(sizeof(stbtt_bakedchar) * FONT_CHAR_COUNT);
  if (stbtt_BakeFontBitmap(tr->ttf_data, 0, font_height, font_pixels,
                           FONT_BITMAP_SIZE, FONT_BITMAP_SIZE, FONT_CHAR_START,
                           FONT_CHAR_COUNT, tr->cdata) <= 0) {
    fprintf(stderr, "Font bitmap too small\n");
    exit_fatal_error();
    return;
  }

  glGenTextures(1, &tr->font_tex);
  glBindTexture(GL_TEXTURE_2D, tr->font_tex);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, FONT_BITMAP_SIZE, FONT_BITMAP_SIZE, 0,
               GL_RED, GL_UNSIGNED_BYTE, font_pixels);
  free_mem(font_pixels);

  glGenVertexArrays(1, &tr->vao);
  glBindVertexArray(tr->vao);

  glGenBuffers(1, &tr->vbo);
  glBindBuffer(GL_ARRAY_BUFFER, tr->vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(QUAD_VERTS), QUAD_VERTS, GL_STATIC_DRAW);

  glGenBuffers(1, &tr->ibo);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, tr->ibo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(QUAD_INDICES), QUAD_INDICES,
               GL_STATIC_DRAW);

  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);

  tr->glyph_program = create_shader_program(GLYPH_VERT_SRC, GLYPH_FRAG_SRC);
}

void text_renderer_destroy(TextRenderer *tr) {
  free_mem(tr->ttf_data);
  tr->ttf_data = NULL;
  free_mem(tr->cdata);
  tr->cdata = NULL;
  glDeleteTextures(1, &tr->font_tex);
  tr->font_tex = 0;
  glDeleteProgram(tr->glyph_program);
  tr->glyph_program = 0;
}

void text_render(TextRenderer *tr, const char *text, float x, float y) {
  glUseProgram(tr->glyph_program);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, tr->font_tex);

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDisable(GL_DEPTH_TEST);

  glDisable(GL_CULL_FACE);

  glBindVertexArray(tr->vao);

  float start_x = x;

  stbtt_aligned_quad quad;
  while (*text) {
    if (*text > FONT_CHAR_START && *text < FONT_CHAR_START + FONT_CHAR_COUNT) {
      stbtt_GetBakedQuad(tr->cdata, FONT_BITMAP_SIZE, FONT_BITMAP_SIZE,
                         *text - FONT_CHAR_START, &x, &y, &quad, 1);
      // OpenGL coordinates are [-1, 1]
      float sizex = (quad.x1 - quad.x0) / (float)global.win_width * 2.0f;
      float sizey = (quad.y1 - quad.y0) / (float)global.win_height * 2.0f;
      float posx =
          -1.0f + ((quad.x0 + quad.x1) * 0.5f) / (float)global.win_width * 2.0f;
      float posy = 1.0f + ((-(quad.y0 + quad.y1) * 0.5f) - tr->font_height * 0.5f) /
                              (float)global.win_height * 2.0f;
      glUniform2f(0, posx, posy);
      glUniform2f(1, sizex, sizey);
      glUniform2f(2, quad.s0, quad.t1);
      glUniform2f(3, quad.s1 - quad.s0, quad.t0 - quad.t1);
      glUniform4f(4, 0.0f, 0.0f, 0.0f, 1.0f);
      glDrawElements(GL_TRIANGLES, sizeof(QUAD_INDICES) / sizeof(*QUAD_INDICES),
                     GL_UNSIGNED_BYTE, NULL);
    } else if (*text == ' ') {
      x += tr->font_height * 0.25f;
    } else if (*text == '\n') {
      x = start_x;
      y += tr->font_height;
    }
    text++;
  }

  glDisable(GL_BLEND);
  glEnable(GL_DEPTH_TEST);

  glEnable(GL_CULL_FACE);
}