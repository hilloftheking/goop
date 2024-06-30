#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "HandmadeMath.h"

#include "fixed_array.h"
#include "blob_defines.h"

typedef enum LiquidType { LIQUID_BASE } LiquidType;

typedef struct SolidBlob {
  float radius;
  HMM_Vec3 pos;
  int mat_idx;
} SolidBlob;

typedef struct LiquidBlob {
  LiquidType type;
  float radius;
  HMM_Vec3 pos;
  int mat_idx;
} LiquidBlob;

typedef struct Projectile Projectile;
typedef struct ColliderModel ColliderModel;
typedef void (*ProjectileCallback)(Projectile *, ColliderModel *, uint64_t);

typedef struct Projectile {
  float radius;
  HMM_Vec3 pos;
  int mat_idx;
  HMM_Vec3 vel;
  ProjectileCallback callback;
  uint64_t userdata;
  float delete_timer;
} Projectile;

typedef struct Model Model;

typedef struct ColliderModel {
  Model *mdl;
} ColliderModel;

typedef enum DeletionType {
  DELETE_SOLID,
  DELETE_LIQUID,
  DELETE_PROJECTILE,
  DELETE_COLLIDER_MODEL,
  DELETE_MAX
} DeletionType;

#define BLOB_SIM_MAX_SOLIDS 1024
#define BLOB_SIM_MAX_LIQUIDS 1024
#define BLOB_SIM_MAX_PROJECTILES 1024
#define BLOB_SIM_MAX_COLLIDER_MODELS 128

#define BLOB_SIM_MAX_DELETIONS 128

// Only a few liquid blobs actually keep track of their velocity
#define BLOB_SIM_MAX_FORCES 128

#define LIQUID_FALL_SPEED 5.0f

// Associates a liquid blob index with a force
typedef struct LiquidForce {
  int idx;
  HMM_Vec3 force;
} LiquidForce;

typedef struct BlobSim {
  FixedArray solids;
  FixedArray liquids;
  FixedArray projectiles;

  FixedArray collider_models;

  FixedArray del_queues[DELETE_MAX];

  LiquidForce *liq_forces;
} BlobSim;

// A blob that belongs to a model
typedef struct ModelBlob {
  float radius;
  HMM_Vec3 pos;
  int mat_idx;
} ModelBlob;

typedef struct Model {
  int blob_count;
  ModelBlob *blobs;
  HMM_Mat4 transform;
} Model;

#define BLOB_RAY_MAX_STEPS 32
#define BLOB_RAY_INTERSECT 0.001f

typedef struct RaycastResult {
  bool has_hit;
  HMM_Vec3 hit;
  HMM_Vec3 norm;
  float traveled;
} RaycastResult;

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

#define BLOB_DEFAULT_RADIUS 0.5f
#define PROJECTILE_DEFAULT_DELETE_TIME 5.0f

// How much force is needed to attract b to other
HMM_Vec3 blob_get_attraction_to(LiquidBlob *b, LiquidBlob *other);

// How much anti gravitational force is applied to b from other
float blob_get_support_with(LiquidBlob *b, LiquidBlob *other);

// Creates a solid blob if possible and adds it to the simulation. The
// returned pointer may not always be valid.
SolidBlob *solid_blob_create(BlobSim *bs);

// Creates a liquid blob if possible and adds it to the simulation. The
// returned pointer may not always be valid.
LiquidBlob *liquid_blob_create(BlobSim *bs);

// Creates a projectile if possible and adds it to the simulation. The returned
// pointer may not always be valid.
Projectile *projectile_create(BlobSim *bs);

// Creates a collider model if possible and adds it to the simulation. The
// returned pointer may not always be valid.
ColliderModel *collider_model_add(BlobSim *bs, Model *mdl);

// Removes the collider model that uses the same model
void collider_model_remove(BlobSim *bs, Model *mdl);

void blob_sim_create(BlobSim *bs);
void blob_sim_destroy(BlobSim *bs);

// Queues a blob to be deleted at the end of a simulation tick. It is fine to
// call this multiple times for the same blob
void blob_sim_queue_delete(BlobSim *bs, DeletionType type, void *b);

void blob_sim_liquid_apply_force(BlobSim *bs, const LiquidBlob *b,
                                 const HMM_Vec3 *force);

// rd should not be normalized
void blob_sim_raycast(RaycastResult *r, const BlobSim *bs, HMM_Vec3 ro, HMM_Vec3 rd);

void blob_simulate(BlobSim *bs, double delta);

void blob_mdl_create(Model *mdl, const ModelBlob *mdl_blob_src,
                     int mdl_blob_count);

// Returns correction vector to separate a blob at pos from solids
HMM_Vec3 blob_get_correction_from_solids(BlobSim *bs, const HMM_Vec3 *pos,
                                         float radius);

// Returns how many bytes are needed to fit the worst case octree
int blob_ot_get_alloc_size();

// Allocates an octree with the max possible size
BlobOt blob_ot_create();

void blob_ot_reset(BlobOt blob_ot);

void blob_ot_insert(BlobOt blob_ot, const HMM_Vec3 *blob_pos, float blob_radius,
                    int blob_idx);