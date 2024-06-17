#pragma once

#include "linmath.h"

typedef struct Skybox {
  unsigned int tex, program;
} Skybox;

void skybox_create(Skybox *sb);

void skybox_draw(const Skybox *sb, const mat4x4 view_mat,
                 const mat4x4 proj_mat);

void skybox_destroy(Skybox *sb);