#include <stdint.h>

#include <glad/glad.h>

#include "HandmadeMath.h"

#include "primitives.h"

static const HMM_Vec3 CUBE_VERTS[] = {
    {0.5f, 0.5f, -0.5f}, {0.5f, -0.5f, -0.5f}, {0.5f, 0.5f, 0.5f},
    {0.5f, -0.5f, 0.5f}, {-0.5f, 0.5f, -0.5f}, {-0.5f, -0.5f, -0.5f},
    {-0.5f, 0.5f, 0.5f}, {-0.5f, -0.5f, 0.5f}};
static const uint8_t CUBE_INDICES[] = {4, 2, 0, 2, 7, 3, 6, 5, 7, 1, 7, 5,
                                       0, 3, 1, 4, 1, 5, 4, 6, 2, 2, 6, 7,
                                       6, 4, 5, 1, 3, 7, 0, 2, 3, 4, 0, 1};

static const HMM_Vec2 QUAD_VERTS[] = {
    {-0.5f, -0.5f}, {0.5f, -0.5f}, {0.5f, 0.5f}, {-0.5f, 0.5f}};
static const uint8_t QUAD_INDICES[] = {0, 1, 2, 2, 3, 0};

unsigned int cube_vao, cube_vbo, cube_ibo, quad_vao, quad_vbo, quad_ibo;

void primitives_create_buffers() {
  glGenVertexArrays(1, &cube_vao);
  glBindVertexArray(cube_vao);

  glGenBuffers(1, &cube_vbo);
  glBindBuffer(GL_ARRAY_BUFFER, cube_vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(CUBE_VERTS), CUBE_VERTS, GL_STATIC_DRAW);

  glGenBuffers(1, &cube_ibo);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cube_ibo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(CUBE_INDICES), CUBE_INDICES,
               GL_STATIC_DRAW);

  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, NULL);

  glGenVertexArrays(1, &quad_vao);
  glBindVertexArray(quad_vao);

  glGenBuffers(1, &quad_vbo);
  glBindBuffer(GL_ARRAY_BUFFER, quad_vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(QUAD_VERTS), QUAD_VERTS, GL_STATIC_DRAW);

  glGenBuffers(1, &quad_ibo);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, quad_ibo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(QUAD_INDICES), QUAD_INDICES,
               GL_STATIC_DRAW);

  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);
}

void cube_draw() {
  glBindVertexArray(cube_vao);
  glDrawElements(GL_TRIANGLES, sizeof(CUBE_INDICES) / sizeof(*CUBE_INDICES),
                 GL_UNSIGNED_BYTE, NULL);
}

void quad_draw() {
  glBindVertexArray(quad_vao);
  glDrawElements(GL_TRIANGLES, sizeof(QUAD_INDICES) / sizeof(*QUAD_INDICES),
                 GL_UNSIGNED_BYTE, NULL);
}