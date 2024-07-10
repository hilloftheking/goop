#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "HandmadeMath.h"

#include "blob_defines.h"
#include "ecs.h"
#include "fixed_array.h"
#include "int_map.h"

typedef enum LiquidType { LIQUID_BASE, LIQUID_PROJ } LiquidType;

typedef struct SolidBlob {
  float radius;
  HMM_Vec3 pos;
  int mat_idx;
} SolidBlob;

typedef struct LiquidBlob LiquidBlob;
typedef struct ColliderModel ColliderModel;
typedef void (*ProjectileCallback)(LiquidBlob *, ColliderModel *);

typedef struct LiquidBlob {
  LiquidType type;
  float radius;
  HMM_Vec3 pos;
  HMM_Vec3 vel;
  int mat_idx;
  union {
    struct {
      ProjectileCallback callback;
      uint64_t userdata;
    } proj;
  };
} LiquidBlob;

typedef struct ColliderModel {
  Entity ent;
} ColliderModel;

typedef enum RemovalType {
  REMOVE_SOLID,
  REMOVE_LIQUID,
  REMOVE_COLLIDER_MODEL,
  REMOVE_MAX
} RemovalType;

typedef struct BlobRemoval {
  int bidx;
  double timer;
} BlobRemoval;

#define BLOB_SIM_MAX_SOLIDS 4096
#define BLOB_SIM_MAX_LIQUIDS 4096
#define BLOB_SIM_MAX_COLLIDER_MODELS 128

#define BLOB_SIM_MAX_DELETIONS 1024

typedef struct BlobOtNode {
  // Blob count if this node is a leaf. Otherwise, it is -1
  int leaf_blob_count;
  // Indices of blobs if this is a leaf. Otherwise, it is the 8 offsets from this
  // node to its children
  int offsets[];
} BlobOtNode;

typedef struct BlobOt BlobOt;

// Octree
typedef struct BlobOt {
  BlobOtNode *root;
  // At what distance should a blob be added to a leaf? This is useful for
  // calculating a signed distance field. Default is 0
  float max_dist_to_leaf;

  int max_subdiv;
  HMM_Vec3 root_pos;
  float root_size;

  // Current capacity in ints (sizeof(BlobOtNode) == sizeof(int))
  int capacity_int;
  int size_int;

  void *userdata;
  const HMM_Vec3 *(*get_pos_from_idx)(BlobOt *, int);
  float (*get_radius_from_idx)(BlobOt *, int);
} BlobOt;

typedef struct BlobOtEnumData BlobOtEnumData;

typedef bool (*BlobOtEnumLeafFunc)(BlobOtEnumData *);

typedef struct BlobOtEnumData {
  BlobOt *bot;
  // Position of sphere or cube being used to find leaves
  HMM_Vec3 shape_pos;
  // Size/radius of sphere/cube
  float shape_size;
  BlobOtEnumLeafFunc callback;
  void *user_data;
  // This is modified by the enum function
  BlobOtNode *curr_leaf;
  int curr_leaf_depth;
  const HMM_Vec3 *curr_leaf_pos;
  float curr_leaf_size;
  BlobOtNode **node_stack;
} BlobOtEnumData;

typedef struct BlobSim {
  FixedArray solids;
  FixedArray liquids;

  FixedArray collider_models;

  FixedArray del_queues[REMOVE_MAX];

  HMM_Vec3 active_pos;
  
  // Used for collision detection and rendering
  BlobOt solid_ot;

  // Used for simulation and rendering
  BlobOt liquid_ot;

  // Used to avoid modifying the same octree while traversing it
  BlobOt liquid_temp_ot;
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
} Model;

typedef struct RaycastResult {
  bool has_hit;
  HMM_Vec3 hit;
  HMM_Vec3 norm;
  float traveled;
} RaycastResult;

// Size of level cube. Contains inactive blobs
static const float BLOB_LEVEL_SIZE = 512.0f;
// Size of active simulation cube
static const float BLOB_ACTIVE_SIZE = 32.0f;

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
LiquidBlob *projectile_create(BlobSim *bs);

// Updates a solid's radius and position, and updates the octree
void solid_blob_set_radius_pos(BlobSim *bs, SolidBlob *b, float radius,
                               const HMM_Vec3 *pos);

// Updates a solid's radius and position, and updates the octree
void liquid_blob_set_radius_pos(BlobSim *bs, LiquidBlob *b, float radius,
                                const HMM_Vec3 *pos);

// Creates a collider model if possible and adds it to the simulation. The
// returned pointer may not always be valid.
ColliderModel *collider_model_add(BlobSim *bs, Entity ent);

// Removes the collider model corresponding to the entity
void collider_model_remove(BlobSim *bs, Entity ent);

void blob_sim_create(BlobSim *bs);
void blob_sim_destroy(BlobSim *bs);

// Queues a blob to be removed at the end of a simulation tick. It is fine to
// call this multiple times for the same blob
BlobRemoval *blob_sim_queue_remove(BlobSim *bs, RemovalType type, void *b);
BlobRemoval *blob_sim_delayed_remove(BlobSim *bs, RemovalType type, void *b,
                                      double t);

// rd should not be normalized
void blob_sim_raycast(RaycastResult *r, const BlobSim *bs, HMM_Vec3 ro, HMM_Vec3 rd);

void blob_simulate(BlobSim *bs, double delta);

void blob_mdl_create(Model *mdl, const ModelBlob *mdl_blob_src,
                     int mdl_blob_count);
void blob_mdl_destroy(Model *mdl);

// Returns correction vector to separate a blob at pos from solids
HMM_Vec3 blob_get_correction_from_solids(BlobSim *bs, const HMM_Vec3 *pos,
                                         float radius);

// Returns how many bytes are needed to fit the worst case octree
int blob_ot_get_alloc_size();

// Allocates an octree with the max possible size
void blob_ot_create(BlobOt *bot);

void blob_ot_destroy(BlobOt *bot);

void blob_ot_reset(BlobOt *bot);

void blob_ot_insert(BlobOt *bot, const HMM_Vec3 *bpos, float bradius, int bidx);

void blob_ot_remove(BlobOt *bot, const HMM_Vec3 *bpos, float bradius, int bidx);

void blob_ot_enum_leaves_sphere(BlobOtEnumData *enum_data);

void blob_ot_enum_leaves_cube(BlobOtEnumData *enum_data);
