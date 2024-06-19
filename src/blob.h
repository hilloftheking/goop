#pragma once

#include <stdbool.h>

#include "HandmadeMath.h"

#include "blob_defines.h"

#define BLOB_MAX_COUNT 1024
#define BLOB_DESIRED_DISTANCE 0.4f
#define BLOB_FALL_SPEED 0.5f

#define BLOB_TICK_TIME 0.1

// TODO: Wake up sleeping blobs when blob character is near
// #define BLOB_SLEEP_ENABLED

// How many ticks until a blob can go to sleep
#define BLOB_SLEEP_TICKS_REQUIRED 10

// How weak attraction and movement must be before sleeping is allowed
#define BLOB_SLEEP_THRESHOLD 0.008f

#define BLOB_SPAWN_CD 0.05

typedef enum BlobType { BLOB_LIQUID = 0, BLOB_SOLID = 1 } BlobType;

typedef struct Blob {
  BlobType type;
  float radius;
  HMM_Vec3 pos;
  HMM_Vec3 prev_pos; // For interpolation
  int mat_idx;
  union {
    int liquid_sleep_ticks;
  };
} Blob;

#define MODEL_BLOB_MAX_COUNT 256

// A blob that belongs to a model
typedef struct ModelBlob {
  float radius;
  HMM_Vec3 pos;
  int mat_idx;
} ModelBlob;

typedef struct BlobSim {
  int blob_count;
  Blob *blobs;

  int model_blob_count;
  ModelBlob *model_blobs;

  double tick_timer;
} BlobSim;

// Models just provide an index into a simulation's model_blobs
typedef struct Model {
  // Index of first model blob
  int idx;
  // Model blob count
  int count;

  HMM_Mat4 transform;
} Model;

#define BLOB_OT_MAX_SUBDIVISIONS 3
// This uses a lot of memory
#define BLOB_OT_LEAF_MAX_BLOB_COUNT 512

typedef struct BlobOtNode {
  // Blob count if this node is a leaf. Otherwise, it is -1
  int leaf_blob_count;
  // Indices of blobs if this is a leaf. Otherwise, it is the 8 indices into
  // the octree of this node's children
  int indices[];
} BlobOtNode;

typedef BlobOtNode *BlobOt;

static const HMM_Vec3 BLOB_SIM_POS = {0, 8, 0};
static const float BLOB_SIM_SIZE = 16.0f;

// How much force is needed to attract b to other
HMM_Vec3 blob_get_attraction_to(Blob *b, Blob *other);

// How much anti gravitational force is applied to b from other
float blob_get_support_with(Blob *b, Blob *other);

// Returns true if a blob is solid
bool blob_is_solid(Blob *b);

// Creates a blob if possible and adds it to the simulation
Blob *blob_create(BlobSim *bs, BlobType type, float radius,
                  const HMM_Vec3 *pos, int mat_idx);

// Creates a model blob if possible and adds it to the simulation
ModelBlob *model_blob_create(BlobSim *bs, float radius,
                             const HMM_Vec3 *pos, int mat_idx);

void blob_sim_create(BlobSim *bs);
void blob_sim_destroy(BlobSim *bs);

void blob_sim_create_mdl(Model *mdl, BlobSim *bs, const ModelBlob *mdl_blob_src,
                         int mdl_blob_count);

void blob_simulate(BlobSim *bs, double delta);

// Returns correction vector to separate a blob at pos from solids
HMM_Vec3 blob_get_correction_from_solids(BlobSim *bs,
                                         const HMM_Vec3 *pos, float radius,
                                         bool check_models);

// Returns how many bytes are needed to fit the worst case octree
int blob_ot_get_alloc_size();

// Allocates an octree with the max possible size
BlobOt blob_ot_create();

void blob_ot_reset(BlobOt blob_ot);

void blob_ot_insert(BlobOt blob_ot, const HMM_Vec3 *blob_pos, int blob_idx);