#pragma once

#include "linmath.h"

#include "blob_defines.h"

#define BLOB_START_COUNT 200
#define BLOB_MAX_COUNT 1024
#define BLOB_DESIRED_DISTANCE 0.4f
#define BLOB_FALL_SPEED 0.5f

#define BLOB_SLEEP_ENABLED

// How many ticks until a blob can go to sleep
#define BLOB_SLEEP_TICKS_REQUIRED 10

// How weak attraction and movement must be before sleeping is allowed
#define BLOB_SLEEP_THRESHOLD 0.008f

#define BLOB_SPAWN_CD 0.05

typedef struct Blob {
  vec3 pos;
  int sleep_ticks;
} Blob;

#define BLOB_OT_MAX_SUBDIVISIONS 3
#define BLOB_OT_LEAF_MAX_BLOB_COUNT 64

typedef struct BlobOtNode {
  // Blob count if this node is a leaf. Otherwise, it is -1
  int leaf_blob_count;
  // Indices of blobs if this is a leaf. Otherwise, it is the 8 indices into
  // the octree of this node's children
  int indices[];
} BlobOtNode;

typedef BlobOtNode *BlobOt;

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

// Returns how many bytes are needed to fit the worst case octree
int blob_ot_get_alloc_size();

// Allocates an octree with the max possible size
BlobOt blob_ot_create();

void blob_ot_reset(BlobOt blob_ot);

void blob_ot_insert(BlobOt blob_ot, vec3 blob_pos, int blob_idx);