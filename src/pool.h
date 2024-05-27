#pragma once

#include <stdbool.h>

#include "linmath.h"

#define POOL_MAX_SOLIDS 32

// A pool is a large box that encompasses many solids (blobs will be supported later)
typedef struct Pool {
  vec3 pos;
  float radius;
  int solid_indices[POOL_MAX_SOLIDS];
  int solid_count;
} Pool;

typedef struct Solid Solid;

void pool_create(Pool *pool, const vec3 pos, float radius);
bool pool_contains_solid(const Pool *pool, const Solid *solid);