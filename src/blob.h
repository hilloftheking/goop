#pragma once

#include "linmath.h"

#define BLOB_COUNT 400
#define BLOB_DESIRED_DISTANCE 0.4f
#define BLOB_FALL_SPEED 0.6f

#define BLOB_SLEEP_ENABLED

// How many ticks until a blob can go to sleep
#define BLOB_SLEEP_TICKS_REQUIRED 10

// How weak attraction and movement must be before sleeping is allowed
#define BLOB_SLEEP_THRESHOLD 0.008f

typedef struct Blob {
  vec3 pos;
  int sleep_ticks;
} Blob;

static vec3 blob_min_pos = {-20.0f, 0.0f, -20.0f};
static vec3 blob_max_pos = {20.0f, 20.0f, 20.0f};

void blob_get_attraction_with(vec3 r, Blob *b, Blob *other);