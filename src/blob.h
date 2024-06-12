#pragma once

#include <stdbool.h>

#include "linmath.h"

#include "blob_defines.h"

#define BLOB_START_COUNT 128
#define BLOB_MAX_COUNT 1024
#define BLOB_CHAR_MAX_COUNT 64
#define BLOB_DESIRED_DISTANCE 0.4f
#define BLOB_FALL_SPEED 0.5f

#define BLOB_TICK_TIME 0.1

// TODO: Wake up sleeping blobs when blob character is near
//#define BLOB_SLEEP_ENABLED

// How many ticks until a blob can go to sleep
#define BLOB_SLEEP_TICKS_REQUIRED 10

// How weak attraction and movement must be before sleeping is allowed
#define BLOB_SLEEP_THRESHOLD 0.008f

#define BLOB_SPAWN_CD 0.05

typedef enum BlobType { BLOB_LIQUID = 0, BLOB_SOLID = 1 } BlobType;

typedef struct Blob {
  BlobType type;
  float radius;
  vec3 pos;
  vec3 prev_pos; // For interpolation
  int mat_idx;
  union {
    int liquid_sleep_ticks;
  };
} Blob;

// A blob that is moved manually
typedef struct BlobChar {
  float radius;
  vec3 pos;
  int mat_idx;
} BlobChar;

typedef struct BlobSimulation {
  int blob_count;
  Blob *blobs;
  int blob_char_count;
  BlobChar *blob_chars;
  double tick_timer;
} BlobSimulation;

#define BLOB_OT_MAX_SUBDIVISIONS 3
#define BLOB_OT_LEAF_MAX_BLOB_COUNT 128

typedef struct BlobOtNode {
  // Blob count if this node is a leaf. Otherwise, it is -1
  int leaf_blob_count;
  // Indices of blobs if this is a leaf. Otherwise, it is the 8 indices into
  // the octree of this node's children
  int indices[];
} BlobOtNode;

typedef BlobOtNode *BlobOt;

static const vec3 blob_min_pos = {-8.0f, 0.0f, -8.0f};
static const vec3 blob_max_pos = {8.0f, 16.0f, 8.0f};

static const vec3 blob_sdf_size = {BLOB_SDF_SIZE};
static const vec3 blob_sdf_start = {BLOB_SDF_START};

// How much force is needed to attract b to other
void blob_get_attraction_to(vec3 r, Blob *b, Blob *other);

// How much anti gravitational force is applied to b from other
float blob_get_support_with(Blob *b, Blob *other);

// Returns true if a blob is solid
bool blob_is_solid(Blob *b);

// Creates a blob if possible and adds it to the simulation
Blob *blob_create(BlobSimulation *bs, BlobType type, float radius,
                 const vec3 pos, int mat_idx);

// Creates a blob character if possible and adds it to the simulation
BlobChar *blob_char_create(BlobSimulation *bs, float radius, const vec3 pos,
                           int mat_idx);

void blob_simulation_create(BlobSimulation *bs);
void blob_simulation_destroy(BlobSimulation *bs);

void blob_simulate(BlobSimulation *bs, double delta);

// Returns correction vector to separate a blob at pos from solids
void blob_get_correction_from_solids(vec3 correction, BlobSimulation *bs,
                                     const vec3 pos, float radius);

// Attempts to move a blob character and does correction against solids
void blob_char_move(BlobSimulation *bs, BlobChar *b, vec3 move);

// Returns how many bytes are needed to fit the worst case octree
int blob_ot_get_alloc_size();

// Allocates an octree with the max possible size
BlobOt blob_ot_create();

void blob_ot_reset(BlobOt blob_ot);

void blob_ot_insert(BlobOt blob_ot, vec3 blob_pos, int blob_idx);