#pragma once

#include "HandmadeMath.h"

typedef struct Skybox {
  unsigned int tex, program;
} Skybox;

void skybox_create(Skybox *sb);

void skybox_draw(const Skybox *sb, const HMM_Mat4 *view_mat,
                 const HMM_Mat4 *proj_mat);

void skybox_destroy(Skybox *sb);