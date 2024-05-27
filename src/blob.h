#pragma once

#include "linmath.h"

#define BLOB_COUNT 400
#define BLOB_DESIRED_DISTANCE 0.4f
#define BLOB_FALL_SPEED 0.4f

typedef struct Blob {
  vec3 pos;
  float unused;
} Blob;

static vec3 blob_min_pos = {-3.0f, 0.0f, -3.0f};
static vec3 blob_max_pos = {3.0f, 20.0f, 3.0f};

void blob_get_attraction_with(vec3 r, Blob *b, Blob *other);