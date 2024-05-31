#pragma once

#include "linmath.h"

#include "blob_defines.h"

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

static vec3 blob_min_pos = {-8.0f, 0.0f, -8.0f};
static vec3 blob_max_pos = {8.0f, 16.0f, 8.0f};

static vec3 blob_sdf_size = {BLOB_SDF_SIZE};
static vec3 blob_sdf_start = {BLOB_SDF_START};

// How much force is needed to attract b to other
void blob_get_attraction_to(vec3 r, Blob *b, Blob *other);

// How much anti gravitational force is applied to b from other
float blob_get_support_with(Blob *b, Blob *other);

// Simulate 1 tick for amount blobs
void simulate_blobs(Blob *blobs, int amount, vec3 *blobs_prev_pos);