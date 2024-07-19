#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "blob.h"
#include "core.h"

#define LIQUID_GRAVITY 9.81f
#define LIQUID_MIN_Y_VEL -10.0f
#define LIQUID_DRAG 0.5f

#define BLOB_RAY_MAX_STEPS 32
#define BLOB_RAY_INTERSECT 0.001f

// TODO: dynamically increase leaf blob count
#define BLOB_OT_LEAF_MAX_BLOB_COUNT 256
#define BLOB_OT_LEAF_SUBDIV_BLOB_COUNT 8
#define BLOB_OT_DEFAULT_CAPACITY_INT 2400000

#define BLOB_DEFAULT_RADIUS 0.5f
#define PROJECTILE_DEFAULT_DELETE_TIME 2.0f

HMM_Vec3 blob_get_attraction_to(LiquidBlob *b, LiquidBlob *other) {
  HMM_Vec3 direction = HMM_SubV3(b->pos, other->pos);
  float len = HMM_LenV3(direction);
  if (len == 0.0f)
    return (HMM_Vec3){0};

  float desired_dist = (b->radius + other->radius) * 0.4f;

  float x = fabsf(desired_dist - len);

  if (x > desired_dist * 2.0f) {
    return (HMM_Vec3){0};
  }

  float power =
      fmaxf(0.0f, logf((-50.0f * fmaxf(1.0f, (other->radius / 0.5f))) * x *
                           (x - desired_dist) +
                       1.0f));

  direction = HMM_MulV3F(HMM_NormV3(direction), power);

  // Blobs only fall with gravity, they don't force each other to fall
  if (direction.Y < 0.0f) {
    direction.Y = 0.0f;
  }

  return direction;
}

float blob_get_support_with(LiquidBlob *b, LiquidBlob *other) {
  return 0.0f;

  HMM_Vec3 direction = HMM_SubV3(b->pos, other->pos);
  float len = HMM_LenV3(direction);

  float desired_dist = (b->radius + other->radius) * 0.4f;

  float x = desired_dist - len;

  if (fabsf(x) > desired_dist * 2.0f) {
    return 0.0f;
  }

  // More support when close together
  if (x > 0.0f) {
    x *= 0.1f;
  }

  // Less support when other blob is above
  if (direction.Y <= 0.1f) {
    x *= 1.4f;
  }

  return fmaxf(0.0f,
               (-1.0f * fmaxf(1.0f, other->radius / 0.5f)) * x * x + 0.4f);
}

SolidBlob *solid_blob_create(BlobSim *bs) {
  SolidBlob *b = fixed_array_append(&bs->solids, NULL);
  if (!b) {
    fprintf(stderr, "Solid blob max count reached\n");
    return NULL;
  }

  b->radius = BLOB_DEFAULT_RADIUS;
  b->pos = HMM_V3(INFINITY, INFINITY, INFINITY);
  b->mat_idx = 0;

  return b;
}

LiquidBlob *liquid_blob_create(BlobSim *bs) {
  LiquidBlob *b = fixed_array_append(&bs->liquids, NULL);
  if (!b) {
    fprintf(stderr, "Liquid blob max count reached\n");
    return NULL;
  }

  b->type = LIQUID_BASE;
  b->radius = BLOB_DEFAULT_RADIUS;
  b->pos = HMM_V3(INFINITY, INFINITY, INFINITY);
  b->vel = HMM_V3(0, 0, 0);
  b->mat_idx = 0;

  return b;
}

LiquidBlob *projectile_create(BlobSim *bs) {
  LiquidBlob *p = liquid_blob_create(bs);

  if (p) {
    p->type = LIQUID_PROJ;
    p->proj.callback = NULL;
    p->proj.userdata = 0;
  }

  return p;
}

void solid_blob_set_radius_pos(BlobSim *bs, SolidBlob *b, float radius,
                               const HMM_Vec3 *pos) {
  int blob_idx = fixed_array_get_idx_from_ptr(&bs->solids, b);

  if (!HMM_EqV3(b->pos, HMM_V3(INFINITY, INFINITY, INFINITY))) {
    blob_ot_remove(&bs->solid_ot, &b->pos, b->radius, blob_idx);
  }
  b->radius = radius;
  b->pos = *pos;
  blob_ot_insert(&bs->solid_ot, pos, radius, blob_idx);
}

void liquid_blob_set_radius_pos(BlobSim *bs, LiquidBlob *b, float radius,
                                const HMM_Vec3 *pos) {
  int blob_idx = fixed_array_get_idx_from_ptr(&bs->liquids, b);

  if (!HMM_EqV3(b->pos, HMM_V3(INFINITY, INFINITY, INFINITY))) {
    blob_ot_remove(&bs->liquid_ot, &b->pos, b->radius, blob_idx);
  }
  b->radius = radius;
  b->pos = *pos;
  blob_ot_insert(&bs->liquid_ot, pos, radius, blob_idx);
}

ColliderModel *collider_model_add(BlobSim *bs, Entity ent) {
  ColliderModel *cm = fixed_array_append(&bs->collider_models, NULL);
  if (!cm) {
    fprintf(stderr, "Collider Model max count reached\n");
    return NULL;
  }

  cm->ent = ent;

  return cm;
}

void collider_model_remove(BlobSim *bs, Entity ent) {
  for (int i = 0; i < bs->collider_models.count; i++) {
    ColliderModel *cm = fixed_array_get(&bs->collider_models, i);
    if (cm->ent == ent) {
      blob_sim_queue_remove(bs, REMOVE_COLLIDER_MODEL, cm);
      return;
    }
  }
}

static const HMM_Vec3 *solid_ot_get_pos_from_idx(BlobOt *bot, int blob_idx) {
  BlobSim *bs = bot->userdata;
  SolidBlob *b = fixed_array_get(&bs->solids, blob_idx);
  return &b->pos;
}

static float solid_ot_get_radius_from_idx(BlobOt *bot, int blob_idx) {
  BlobSim *bs = bot->userdata;
  SolidBlob *b = fixed_array_get(&bs->solids, blob_idx);
  return b->radius;
}

static const HMM_Vec3 *liquid_ot_get_pos_from_idx(BlobOt *bot, int blob_idx) {
  BlobSim *bs = bot->userdata;
  LiquidBlob *b = fixed_array_get(&bs->liquids, blob_idx);
  return &b->pos;
}

static float liquid_ot_get_radius_from_idx(BlobOt *bot, int blob_idx) {
  BlobSim *bs = bot->userdata;
  LiquidBlob *b = fixed_array_get(&bs->liquids, blob_idx);
  return b->radius;
}

void blob_sim_create(BlobSim *bs) {
  fixed_array_create(&bs->solids, sizeof(SolidBlob), BLOB_SIM_MAX_SOLIDS);
  fixed_array_create(&bs->liquids, sizeof(LiquidBlob), BLOB_SIM_MAX_LIQUIDS);
  fixed_array_create(&bs->collider_models, sizeof(ColliderModel),
                     BLOB_SIM_MAX_COLLIDER_MODELS);

  for (int i = 0; i < REMOVE_MAX; i++) {
    fixed_array_create(&bs->del_queues[i], sizeof(BlobRemoval),
                       BLOB_SIM_MAX_DELETIONS);
  }

  bs->active_pos = HMM_V3(0, 0, 0);

  bs->solid_ot.max_subdiv = 8;
  bs->solid_ot.root_pos = HMM_V3(0, 0, 0);
  bs->solid_ot.root_size = BLOB_LEVEL_SIZE;
  bs->solid_ot.userdata = bs;
  bs->solid_ot.get_pos_from_idx = solid_ot_get_pos_from_idx;
  bs->solid_ot.get_radius_from_idx = solid_ot_get_radius_from_idx;
  blob_ot_create(&bs->solid_ot);
  // This octree also gets sent to the GPU
  bs->solid_ot.max_dist_to_leaf = BLOB_SDF_MAX_DIST;

  bs->liquid_ot.max_subdiv = 8;
  bs->liquid_ot.root_pos = HMM_V3(0, 0, 0);
  bs->liquid_ot.root_size = BLOB_LEVEL_SIZE;
  bs->liquid_ot.userdata = bs;
  bs->liquid_ot.get_pos_from_idx = liquid_ot_get_pos_from_idx;
  bs->liquid_ot.get_radius_from_idx = liquid_ot_get_radius_from_idx;
  blob_ot_create(&bs->liquid_ot);
  bs->liquid_ot.max_dist_to_leaf = BLOB_SDF_MAX_DIST;

  bs->liquid_temp_ot = bs->liquid_ot;
  blob_ot_create(&bs->liquid_temp_ot);
  bs->liquid_temp_ot.max_dist_to_leaf = BLOB_SDF_MAX_DIST;
}

void blob_sim_destroy(BlobSim *bs) {
  fixed_array_destroy(&bs->solids);
  fixed_array_destroy(&bs->liquids);
  fixed_array_destroy(&bs->collider_models);

  for (int i = 0; i < REMOVE_MAX; i++) {
    fixed_array_destroy(&bs->del_queues[i]);
  }

  blob_ot_destroy(&bs->solid_ot);
  blob_ot_destroy(&bs->liquid_ot);
  blob_ot_destroy(&bs->liquid_temp_ot);
}

BlobRemoval *blob_sim_queue_remove(BlobSim *bs, RemovalType type, void *b) {
  BlobRemoval *del = fixed_array_append(&bs->del_queues[type], NULL);
  if (!del) {
    fprintf(stderr, "Too many blobs being deleted\n");
    return NULL;
  }

  del->timer = 0.0;

  switch (type) {
  case REMOVE_SOLID:
    del->bidx = fixed_array_get_idx_from_ptr(&bs->solids, b);
    break;
  case REMOVE_LIQUID:
    del->bidx = fixed_array_get_idx_from_ptr(&bs->liquids, b);
    break;
  case REMOVE_COLLIDER_MODEL:
    del->bidx = fixed_array_get_idx_from_ptr(&bs->collider_models, b);
    break;
  case REMOVE_MAX:
    break;
  }

  return del;
}

BlobRemoval *blob_sim_delayed_remove(BlobSim *bs, RemovalType type, void *b,
                                     double t) {
  BlobRemoval *del = blob_sim_queue_remove(bs, type, b);
  if (del) {
    del->timer = t;
  }
  return del;
}

static float clampf(float x, float a, float b) {
  return x > a ? x < b ? x : b : a;
}

static float mixf(float x, float y, float a) { return x * (1.0f - a) + y * a; }

static float sminf(float a, float b, float k) {
  float h = clampf(0.5f + 0.5f * (b - a) / k, 0.0f, 1.0f);
  return mixf(b, a, h) - k * h * (1.0f - h);
}

static float dist_sphere(const HMM_Vec3 *c, float r, const HMM_Vec3 *p) {
  HMM_Vec3 d = HMM_SubV3(*p, *c);
  return HMM_LenV3(d) - r;
}

void blob_sim_raycast(RaycastResult *r, const BlobSim *bs, HMM_Vec3 ro,
                      HMM_Vec3 rd) {
  r->has_hit = false;

  float ray_dist = HMM_LenV3(rd);
  if (ray_dist == 0.0f) {
    return;
  }
  HMM_Vec3 rd_n = HMM_NormV3(rd);

  float traveled = 0.0f;

  float closest_sphere = 1000000.0f;

  for (int i = 0; i < BLOB_RAY_MAX_STEPS; i++) {
    HMM_Vec3 p = HMM_AddV3(ro, HMM_MulV3F(rd_n, traveled));

    float dist = 100000.0f;
    for (int j = 0; j < bs->solids.count; j++) {
      const SolidBlob *b = fixed_array_get_const(&bs->solids, j);
      float d = dist_sphere(&b->pos, b->radius, &p);
      dist = sminf(dist, dist_sphere(&b->pos, b->radius, &p), BLOB_SMOOTH);
      if (d < closest_sphere) {
        closest_sphere = d;
        r->blob_idx = j;
      }
    }

    traveled += dist;

    if (dist <= BLOB_RAY_INTERSECT) {
      r->has_hit = true;
      r->hit = p;
      r->traveled = traveled;
    } else if (traveled >= ray_dist) {
      break;
    }
  }
}

typedef struct SimulationLiquidData {
  BlobSim *bs;
  double delta;
  // Keep track of which liquids have been simulated
  bool simulated[BLOB_SIM_MAX_LIQUIDS];
} SimulationLiquidData;

static bool blob_simulate_liquid_ot_leaf(BlobOtEnumData *enum_data) {
  BlobOtNode *leaf = enum_data->curr_leaf;
  SimulationLiquidData *sim_data = enum_data->user_data;
  BlobSim *bs = sim_data->bs;
  double delta = sim_data->delta;

  for (int i = 0; i < leaf->leaf_blob_count; i++) {
    int bidx = leaf->offsets[i];
    if (sim_data->simulated[bidx]) {
      continue;
    }
    sim_data->simulated[bidx] = true;

    LiquidBlob *b = fixed_array_get(&bs->liquids, bidx);
    HMM_Vec3 new_pos = b->pos;

    float anti_grav = 0.0f;

    // This only works when each liquid blob has a smaller radius than
    // SDF_MAX_DIST
    for (int j = 0; j < leaf->leaf_blob_count; j++) {
      if (j == i)
        continue;

      int o_bidx = leaf->offsets[j];
      LiquidBlob *ob = fixed_array_get(&bs->liquids, o_bidx);

      HMM_Vec3 attraction = blob_get_attraction_to(b, ob);
      b->vel = HMM_AddV3(b->vel, HMM_MulV3F(attraction, (float)delta * 5.0f));
      /*
      anti_grav += blob_get_support_with(b, ob);
      */
    }

    b->vel.Y -= LIQUID_GRAVITY * delta * (1.0f - fminf(anti_grav, 1.0f));
    b->vel.Y = HMM_MAX(b->vel.Y, LIQUID_MIN_Y_VEL);

    // Now position + velocity * delta will be the new position before dealing
    // with solid blobs

    new_pos = HMM_AddV3(b->pos, HMM_MulV3F(b->vel, (float)delta));
    HMM_Vec3 correction =
        blob_get_correction_from_solids(bs, &new_pos, b->radius);
    if (HMM_LenV3(correction) > 0.0f) {
      HMM_Vec3 n = HMM_NormV3(correction);
      HMM_Vec3 u = HMM_MulV3F(n, HMM_DotV3(b->vel, n));
      HMM_Vec3 w = HMM_SubV3(b->vel, u);
      const float f = 0.95f;
      const float r = (b->type == LIQUID_PROJ) ? 0.1f : 0.01f;
      b->vel = HMM_SubV3(HMM_MulV3F(w, f), HMM_MulV3F(u, r));
    }
    new_pos = HMM_AddV3(new_pos, correction);

    if (b->type == LIQUID_BASE) {
      b->vel = HMM_MulV3F(b->vel, 1.0f - LIQUID_DRAG * (float)delta);
    } else if (b->type == LIQUID_PROJ) {
      for (int c = 0; c < bs->collider_models.count; c++) {
        ColliderModel *col_mdl = fixed_array_get(&bs->collider_models, c);
        Model *mdl =
            entity_get_component_or_null(col_mdl->ent, COMPONENT_MODEL);
        if (!mdl)
          continue;

        for (int bi = 0; bi < mdl->blob_count; bi++) {
          ModelBlob *mb = &mdl->blobs[bi];

          HMM_Vec4 mbpv4 = {0, 0, 0, 1};
          mbpv4.XYZ = mb->pos;

          HMM_Mat4 *trans =
              entity_get_component(col_mdl->ent, COMPONENT_TRANSFORM);
          HMM_Vec3 mbpos = HMM_MulM4V4(*trans, mbpv4).XYZ;
          if (HMM_LenV3(HMM_SubV3(mbpos, b->pos)) <= mb->radius + b->radius) {
            if (b->proj.callback) {
              b->proj.callback(b, col_mdl);
            }
            break;
          }
        }
      }
    }

    liquid_blob_set_radius_pos(sim_data->bs, b, b->radius, &new_pos);
  }

  return true;
}

void blob_simulate(BlobSim *bs, double delta) {
  // Liquids
  {
    memcpy(bs->liquid_temp_ot.root, bs->liquid_ot.root,
           bs->liquid_ot.size_int * sizeof(int));
    bs->liquid_temp_ot.size_int = bs->liquid_ot.size_int;

    SimulationLiquidData sim_data = {0};
    sim_data.bs = bs;
    sim_data.delta = delta;

    BlobOtEnumData enum_data;
    enum_data.bot = &bs->liquid_temp_ot;
    enum_data.shape_pos = bs->active_pos;
    enum_data.shape_size = BLOB_ACTIVE_SIZE;
    enum_data.callback = blob_simulate_liquid_ot_leaf;
    enum_data.user_data = &sim_data;

    blob_ot_enum_leaves_cube(&enum_data);
  }

  // Deletion queues
  FixedArray *const blob_arrays[] = {&bs->solids, &bs->liquids,
                                     &bs->collider_models};
  for (int bt = 0; bt < REMOVE_MAX; bt++) {
    FixedArray *ba = blob_arrays[bt];
    FixedArray *del_queue = &bs->del_queues[bt];

    for (int di = 0; di < del_queue->count; di++) {
      BlobRemoval *del = fixed_array_get(del_queue, di);
      del->timer -= delta;
      if (del->timer > 0.0) {
        continue;
      }
      int bidx = del->bidx;

      if (bt == REMOVE_LIQUID) {
        // Remove this blob from the octree and decrement any indices
        // TODO: anything but this
        LiquidBlob *b = fixed_array_get(ba, bidx);
        BlobOt *bot = &bs->liquid_ot;
        blob_ot_remove(bot, &b->pos, b->radius, bidx);
        BlobOtNode *node_stack[BLOB_OT_MAX_SUBDIVISIONS + 1];
        node_stack[0] = bot->root;
        int iter_stack[BLOB_OT_MAX_SUBDIVISIONS + 1];
        iter_stack[0] = 0;
        int depth = 0;

        while (depth >= 0) {
          BlobOtNode *node = node_stack[depth];
          if (node->leaf_blob_count == -1) {
            int *c = &iter_stack[depth];
            if (*c < 8) {
              node_stack[depth + 1] = node + node->offsets[*c];
              iter_stack[depth + 1] = 0;
              depth += 2;
              (*c)++;
            }
          } else {
            for (int lbi = 0; lbi < node->leaf_blob_count; lbi++) {
              int other_idx = node->offsets[lbi];
              if (other_idx > bidx) {
                node->offsets[lbi]--;
              }
            }
          }

          depth--;
        }
      }

      // Decrement blob indices in deletion queue after this one. Or remove it
      // from the queue if it if it is the same as the current blob index
      for (int dj = 0; dj < del_queue->count; dj++) {
        if (dj == di)
          continue;
        BlobRemoval *other_del = fixed_array_get(del_queue, dj);
        if (other_del->bidx > bidx) {
          other_del->bidx--;
        } else if (other_del->bidx == bidx) {
          fixed_array_remove(del_queue, dj);
          if (dj < di) {
            di--;
          }
          dj--;
        }
      }

      fixed_array_remove(ba, bidx);
      fixed_array_remove(del_queue, di);
      di--;
    }
  }
}

void blob_mdl_create(Model *mdl, const ModelBlob *mdl_blob_src,
                     int mdl_blob_count) {
  mdl->blobs = alloc_mem(sizeof(*mdl->blobs) * mdl_blob_count);
  mdl->blob_count = mdl_blob_count;

  for (int i = 0; i < mdl->blob_count; i++) {
    mdl->blobs[i] = mdl_blob_src[i];
  }
}

void blob_mdl_destroy(Model *mdl) {
  free(mdl->blobs);
  mdl->blob_count = 0;
}

static void blob_check_blob_at(float *min_dist, HMM_Vec3 *correction,
                               const HMM_Vec3 *bpos, float bradius,
                               const HMM_Vec3 *pos, float radius,
                               float smooth) {
  *min_dist = sminf(*min_dist, dist_sphere(bpos, bradius, pos), smooth);

  HMM_Vec3 dir = HMM_SubV3(*pos, *bpos);
  float influence = fmaxf(0.0f, radius + bradius + smooth - HMM_LenV3(dir));
  *correction = HMM_AddV3(*correction, HMM_MulV3F(HMM_NormV3(dir), influence));
}

typedef struct CorrectionData {
  BlobSim *bs;
  HMM_Vec3 correction;
  float min_dist;
  // Keep track of which solids have been checked
  bool checked[BLOB_SIM_MAX_SOLIDS];
} CorrectionData;

static bool blob_get_correction_from_solids_ot_leaf(BlobOtEnumData *enum_data) {
  CorrectionData *correction_data = enum_data->user_data;
  for (int i = 0; i < enum_data->curr_leaf->leaf_blob_count; i++) {
    int bidx = enum_data->curr_leaf->offsets[i];
    if (!correction_data->checked[bidx]) {
      correction_data->checked[bidx] = true;
      SolidBlob *b = fixed_array_get(&correction_data->bs->solids, bidx);
      blob_check_blob_at(
          &correction_data->min_dist, &correction_data->correction, &b->pos,
          b->radius, &enum_data->shape_pos, enum_data->shape_size, BLOB_SMOOTH);
    }
  }

  return true;
}

HMM_Vec3 blob_get_correction_from_solids(BlobSim *bs, const HMM_Vec3 *pos,
                                         float radius) {
  CorrectionData correction_data = {0};
  correction_data.bs = bs;
  correction_data.correction = HMM_V3(0, 0, 0);
  correction_data.min_dist = 10000.0f;

  BlobOtEnumData enum_data;
  enum_data.bot = &bs->solid_ot;
  enum_data.shape_pos = *pos;
  enum_data.shape_size = radius;
  enum_data.callback = blob_get_correction_from_solids_ot_leaf;
  enum_data.user_data = &correction_data;

  // Use smin to figure out if this blob is touching smoothed solid blobs
  // Also check how much each blob should influence the normal

  blob_ot_enum_leaves_sphere(&enum_data);

  // Is this blob inside of a solid blob?
  if (correction_data.min_dist < radius) {
    return HMM_MulV3F(HMM_NormV3(correction_data.correction),
                      radius - correction_data.min_dist);
  } else {
    return (HMM_Vec3){0};
  }
}

int blob_ot_get_alloc_size() {
  int current_nodes = 1;
  int total_size = sizeof(BlobOtNode) + sizeof(int[8]);
  for (int i = 0; i < BLOB_OT_MAX_SUBDIVISIONS; i++) {
    current_nodes *= 8;
    int is_leaf = i == BLOB_OT_MAX_SUBDIVISIONS - 1;

    if (!is_leaf) {
      total_size += current_nodes * (sizeof(BlobOtNode) + sizeof(int[8]));
    } else {
      total_size += current_nodes * (sizeof(BlobOtNode) +
                                     sizeof(int[BLOB_OT_LEAF_MAX_BLOB_COUNT]));
    }
  }

  return total_size;
}

void blob_ot_create(BlobOt *bot) {
  _Static_assert(sizeof(BlobOtNode) == sizeof(int),
                 "BlobOtNode should be the same as int");

  bot->size_int = 1 + BLOB_OT_LEAF_MAX_BLOB_COUNT;
  bot->capacity_int = BLOB_OT_DEFAULT_CAPACITY_INT;
  bot->root = alloc_mem(bot->capacity_int * sizeof(int));
  bot->root->leaf_blob_count = 0;
  bot->max_dist_to_leaf = 0.0f;
}

void blob_ot_destroy(BlobOt *bot) {
  free(bot->root);
  bot->root = NULL;
}

void blob_ot_reset(BlobOt *bot) {
  bot->size_int = 1 + BLOB_OT_LEAF_MAX_BLOB_COUNT;
  bot->root->leaf_blob_count = 0;
}

// This is also in the compute shader, so be careful if it needs to be changed
static HMM_Vec3 ot_octants[8] = {{0.5f, 0.5f, 0.5f},   {0.5f, 0.5f, -0.5f},
                                 {0.5f, -0.5f, 0.5f},  {0.5f, -0.5f, -0.5f},
                                 {-0.5f, 0.5f, 0.5f},  {-0.5f, 0.5f, -0.5f},
                                 {-0.5f, -0.5f, 0.5f}, {-0.5f, -0.5f, -0.5f}};

// Can also be used for spheres but it is less accurate. Returns bitflag of
// octant children
static int get_octant_children_containing_cube(const HMM_Vec3 *npos,
                                               const HMM_Vec3 *cpos,
                                               float chalf) {
  int ret = 0xFF;

  float dx = cpos->X - npos->X;
  if (dx >= chalf) {
    ret &= 0x0F;
  } else if (dx <= -chalf) {
    ret &= 0xF0;
  }

  float dy = cpos->Y - npos->Y;
  if (dy >= chalf) {
    ret &= 0x33;
  } else if (dy <= -chalf) {
    ret &= 0xCC;
  }

  float dz = cpos->Z - npos->Z;
  if (dz >= chalf) {
    ret &= 0x55;
  } else if (dz <= -chalf) {
    ret &= 0xAA;
  }

  return ret;
}

static float dist_cube(const HMM_Vec3 *c, float s, const HMM_Vec3 *p) {
  HMM_Vec3 d;
  for (int i = 0; i < 3; i++) {
    d.Elements[i] =
        HMM_MAX(0.0f, HMM_ABS(p->Elements[i] - c->Elements[i]) - s * 0.5f);
  }
  return HMM_LenV3(d);
}

static bool blob_ot_insert_ot_leaf(BlobOtEnumData *enum_data) {
  BlobOtNode *leaf = enum_data->curr_leaf;

  if (leaf->leaf_blob_count >= BLOB_OT_LEAF_MAX_BLOB_COUNT) {
    static bool printed_warning = false;
    if (!printed_warning) {
      printed_warning = true;
      fprintf(stderr, "Leaf node is full!\n");
    }
    return true;
  }

  leaf->offsets[leaf->leaf_blob_count++] = *(int *)enum_data->user_data;

  if (enum_data->curr_leaf_depth < enum_data->bot->max_subdiv &&
      leaf->leaf_blob_count == BLOB_OT_LEAF_SUBDIV_BLOB_COUNT) {
    int reinsert[BLOB_OT_LEAF_SUBDIV_BLOB_COUNT];
    for (int i = 0; i < BLOB_OT_LEAF_SUBDIV_BLOB_COUNT; i++) {
      reinsert[i] = leaf->offsets[i];
    }

    // If the new leaves are not the last subdivision, there should only be a
    // max of BLOB_OT_LEAF_SUBDIV_BLOB_COUNT leaves (fingers crossed)
    int max_blobs_per_child;
    if (enum_data->curr_leaf_depth < enum_data->bot->max_subdiv - 1) {
      max_blobs_per_child = BLOB_OT_LEAF_SUBDIV_BLOB_COUNT;
    } else {
      max_blobs_per_child = BLOB_OT_LEAF_MAX_BLOB_COUNT;
    }

    int old_node_size_int = 1 + BLOB_OT_LEAF_SUBDIV_BLOB_COUNT;
    int new_node_size_int = 1 + 8;
    int children_size_int = (1 + max_blobs_per_child) * 8;
    int size_diff = new_node_size_int + children_size_int - old_node_size_int;

    // Adjust offsets in each parent
    for (int d = enum_data->curr_leaf_depth - 1; d >= 0; d--) {
      BlobOtNode *parent = enum_data->node_stack[d];
      for (int i = 0; i < 8; i++) {
        int *offset = &parent->offsets[i];
        if (parent + *offset > leaf) {
          *offset += size_diff;
        }
      }
    }

    int new_size_int = enum_data->bot->size_int + size_diff;

    if (new_size_int > enum_data->bot->capacity_int) {
      fprintf(stderr, "Octree is too big\n");
      exit_fatal_error();

      printf("Reallocating\n");
      int old_capacity = enum_data->bot->capacity_int;
      int new_capacity = old_capacity * 2;

      void *new_data = alloc_mem(new_capacity * sizeof(int));
      memcpy(new_data, enum_data->bot->root,
             enum_data->bot->size_int * sizeof(int));

      free(enum_data->bot->root);
      enum_data->bot->root = new_data;
      enum_data->bot->capacity_int = new_capacity;
    }

    BlobOtNode *node = leaf; // TODO
    void *src_start = node + old_node_size_int;
    void *src_end = enum_data->bot->root + enum_data->bot->size_int;
    int src_size = (int)((char *)src_end - (char *)src_start);
    memmove(node + new_node_size_int + children_size_int, src_start, src_size);
    enum_data->bot->size_int = new_size_int;
    node->leaf_blob_count = -1;

    // Set up each child right now before inserting nodes into them since they
    // might get filled up as well
    for (int i = 0; i < 8; i++) {
      node->offsets[i] = new_node_size_int + i * (1 + max_blobs_per_child);
      BlobOtNode *child = node + node->offsets[i];
      child->leaf_blob_count = 0;
    }

    // TODO: don't change this stuff, this is nasty
    HMM_Vec3 old_pos = enum_data->shape_pos;
    float old_size = enum_data->shape_size;
    void *old_user_data = enum_data->user_data;

    const HMM_Vec3 *old_leaf_pos = enum_data->curr_leaf_pos;
    float old_leaf_size = enum_data->curr_leaf_size;

    for (int i = 0; i < 8; i++) {
      BlobOtNode *child = node + node->offsets[i];
      HMM_Vec3 child_pos =
          HMM_AddV3(HMM_MulV3F(ot_octants[i], enum_data->curr_leaf_size * 0.5f),
                    *enum_data->curr_leaf_pos);
      float child_size = enum_data->curr_leaf_size * 0.5f;

      for (int j = 0; j < BLOB_OT_LEAF_SUBDIV_BLOB_COUNT; j++) {
        enum_data->shape_pos =
            *enum_data->bot->get_pos_from_idx(enum_data->bot, reinsert[j]);
        enum_data->shape_size =
            enum_data->bot->get_radius_from_idx(enum_data->bot, reinsert[j]) +
            BLOB_SMOOTH + enum_data->bot->max_dist_to_leaf;
        float dist = dist_cube(&child_pos, child_size, &enum_data->shape_pos) -
                     enum_data->shape_size;

        if (dist <= 0.0f) {
          // TODO: enum_data->bot might be invalidated
          enum_data->curr_leaf_depth++;
          enum_data->node_stack[enum_data->curr_leaf_depth] = child;
          enum_data->curr_leaf = child;
          enum_data->curr_leaf_pos = &child_pos;
          enum_data->curr_leaf_size = child_size;
          enum_data->user_data = &reinsert[j];

          // If the child being inserted into also becomes subdivided, it will
          // be fine since reinsertion will be done
          blob_ot_insert_ot_leaf(enum_data);

          enum_data->curr_leaf_depth--;
        }
      }

      enum_data->curr_leaf_pos = old_leaf_pos;
      enum_data->curr_leaf_size = old_leaf_size;
    }

    enum_data->shape_pos = old_pos;
    enum_data->shape_size = old_size;
    enum_data->user_data = old_user_data;
  }

  return true;
}

void blob_ot_insert(BlobOt *bot, const HMM_Vec3 *bpos, float bradius,
                    int bidx) {
  BlobOtEnumData enum_data;
  enum_data.bot = bot;
  enum_data.shape_pos = *bpos;
  enum_data.shape_size = bradius + BLOB_SMOOTH + bot->max_dist_to_leaf;
  enum_data.callback = blob_ot_insert_ot_leaf;
  enum_data.user_data = &bidx;

  blob_ot_enum_leaves_sphere(&enum_data);
}

static bool blob_ot_remove_ot_leaf(BlobOtEnumData *enum_data) {
  BlobOtNode *leaf = enum_data->curr_leaf;
  int bidx = *(int *)enum_data->user_data;

  for (int i = 0; i < leaf->leaf_blob_count; i++) {
    int oidx = leaf->offsets[i];
    if (oidx == bidx) {
      void *dst = leaf->offsets + i;
      void *src = leaf->offsets + i + 1;
      int src_size_bytes = (leaf->leaf_blob_count - i - 1) * sizeof(int);
      memmove(dst, src, src_size_bytes);
      leaf->leaf_blob_count--;
      break;
    }
  }

  return true;
}

void blob_ot_remove(BlobOt *bot, const HMM_Vec3 *bpos, float bradius,
                    int bidx) {
  BlobOtEnumData enum_data;
  enum_data.bot = bot;
  enum_data.shape_pos = *bpos;
  enum_data.shape_size = bradius + BLOB_SMOOTH + bot->max_dist_to_leaf;
  enum_data.callback = blob_ot_remove_ot_leaf;
  enum_data.user_data = &bidx;

  blob_ot_enum_leaves_sphere(&enum_data);
}

void blob_ot_enum_leaves_sphere(BlobOtEnumData *enum_data) {
  BlobOtNode *node_stack[BLOB_OT_MAX_SUBDIVISIONS + 1];
  node_stack[0] = enum_data->bot->root;
  HMM_Vec3 pos_stack[BLOB_OT_MAX_SUBDIVISIONS + 1];
  pos_stack[0] = enum_data->bot->root_pos;
  float size_stack[BLOB_OT_MAX_SUBDIVISIONS + 1];
  size_stack[0] = enum_data->bot->root_size;

  // Have to keep track of how many children have been checked
  int iter_stack[BLOB_OT_MAX_SUBDIVISIONS + 1];
  iter_stack[0] = 0;

  int node_depth = 0;

  while (node_depth >= 0) {
    // Current node in the stack
    BlobOtNode *node = node_stack[node_depth];
    const HMM_Vec3 *npos = &pos_stack[node_depth];
    float nsize = size_stack[node_depth];

    if (node->leaf_blob_count == -1) {
      // This node has child nodes

      int cflag = get_octant_children_containing_cube(
          npos, &enum_data->shape_pos, enum_data->shape_size);

      int *i = &iter_stack[node_depth];
      for (; *i < 8; (*i)++) {
        if ((cflag & (1 << *i)) == 0) {
          continue;
        }

        pos_stack[node_depth + 1] =
            HMM_AddV3(HMM_MulV3F(ot_octants[*i], nsize * 0.5f), *npos);
        size_stack[node_depth + 1] = nsize * 0.5f;

        node_stack[node_depth + 1] = node + node->offsets[*i];
        iter_stack[node_depth + 1] = 0;
        node_depth += 2; // Because there is a decrement at the end of the
                         // outer loop
        (*i)++;          // Don't check this child again
        break;
      }
    } else {
      // This is a leaf node
      enum_data->curr_leaf = node;
      enum_data->curr_leaf_depth = node_depth;
      enum_data->curr_leaf_pos = npos;
      enum_data->curr_leaf_size = nsize;
      enum_data->node_stack = node_stack;
      if (!enum_data->callback(enum_data)) {
        break;
      }
    }

    node_depth--;
  }
}

static bool cube_cube_intersect(const HMM_Vec3 *pos0, float size0,
                                const HMM_Vec3 *pos1, float size1) {
  float hs0 = size0 * 0.5f;
  float hs1 = size1 * 0.5f;

  if (pos0->X + hs0 < pos1->X - hs1 || pos1->X + hs1 < pos0->X - hs0)
    return false;

  if (pos0->Y + hs0 < pos1->Y - hs1 || pos1->Y + hs1 < pos0->Y - hs0)
    return false;

  if (pos0->Z + hs0 < pos1->Z - hs1 || pos1->Z + hs1 < pos0->Z - hs0)
    return false;

  return true;
}

void blob_ot_enum_leaves_cube(BlobOtEnumData *enum_data) {
  BlobOtNode *node_stack[BLOB_OT_MAX_SUBDIVISIONS + 1];
  node_stack[0] = enum_data->bot->root;
  HMM_Vec3 pos_stack[BLOB_OT_MAX_SUBDIVISIONS + 1];
  pos_stack[0] = enum_data->bot->root_pos;
  float size_stack[BLOB_OT_MAX_SUBDIVISIONS + 1];
  size_stack[0] = enum_data->bot->root_size;

  // Have to keep track of how many children have been checked
  int iter_stack[BLOB_OT_MAX_SUBDIVISIONS + 1];
  iter_stack[0] = 0;

  int node_depth = 0;

  while (node_depth >= 0) {
    // Current node in the stack
    BlobOtNode *node = node_stack[node_depth];
    const HMM_Vec3 *npos = &pos_stack[node_depth];
    float nsize = size_stack[node_depth];

    if (node->leaf_blob_count == -1) {
      // This node has child nodes
      int *i = &iter_stack[node_depth];
      for (; *i < 8; (*i)++) {
        pos_stack[node_depth + 1] =
            HMM_AddV3(HMM_MulV3F(ot_octants[*i], nsize * 0.5f), *npos);
        size_stack[node_depth + 1] = nsize * 0.5f;

        bool intersect = cube_cube_intersect(
            &pos_stack[node_depth + 1], size_stack[node_depth + 1],
            &enum_data->shape_pos, enum_data->shape_size);

        if (intersect) {
          node_stack[node_depth + 1] = node + node->offsets[*i];
          iter_stack[node_depth + 1] = 0;
          node_depth += 2; // Because there is a decrement at the end of the
                           // outer loop
          (*i)++;          // Don't check this child again
          break;
        }
      }
    } else {
      // This is a leaf node
      enum_data->curr_leaf = node;
      enum_data->curr_leaf_depth = node_depth;
      enum_data->curr_leaf_pos = npos;
      enum_data->curr_leaf_size = nsize;
      enum_data->node_stack = node_stack;
      if (!enum_data->callback(enum_data)) {
        break;
      }
    }

    node_depth--;
  }
}