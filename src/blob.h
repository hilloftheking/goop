#pragma once

#include "linmath.h"

#define BLOB_COUNT 200
#define BLOB_DESIRED_DISTANCE 0.4f
#define BLOB_FALL_SPEED 0.5f

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

// How much force is needed to attract b to other
void blob_get_attraction_to(vec3 r, Blob *b, Blob *other);

// How much anti gravitational force is applied to b from other
float blob_get_support_with(Blob *b, Blob *other);