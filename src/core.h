#pragma once

#include <stdint.h>

#define ARR_SIZE(a) (sizeof(a) / sizeof(*a))

typedef struct GLFWwindow GLFWwindow;
typedef uint64_t Entity;
typedef struct BlobSim BlobSim;
typedef struct BlobRenderer BlobRenderer;

typedef struct Global {
  GLFWwindow *window;
  Entity player_ent;
  BlobSim *blob_sim;
  BlobRenderer *blob_renderer;
  float cam_rot_x;
  float cam_rot_y;
  double curr_delta;
} Global;

extern Global global;

void exit_fatal_error();

float rand_float();
