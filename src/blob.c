#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "blob.h"

#define LIQUID_FALL_SPEED 5.0f

#define BLOB_RAY_MAX_STEPS 32
#define BLOB_RAY_INTERSECT 0.001f

#define BLOB_OT_MAX_SUBDIVISIONS 4
// TODO: dynamically increase leaf blob count
#define BLOB_OT_LEAF_MAX_BLOB_COUNT 256
#define BLOB_OT_LEAF_SUBDIV_BLOB_COUNT 16
#define BLOB_OT_DEFAULT_CAPACITY_INT 1200000

#define BLOB_DEFAULT_RADIUS 0.5f
#define PROJECTILE_DEFAULT_DELETE_TIME 5.0f

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
  b->pos = HMM_V3(0, 0, 0);
  b->mat_idx = 0;

  return b;
}

void solid_blob_set_radius_pos(BlobSim *bs, SolidBlob *b, float radius,
                               const HMM_Vec3 *pos) {
  b->radius = radius;
  b->pos = *pos;

  int blob_idx = fixed_array_get_idx_from_ptr(&bs->solids, b);
  blob_ot_insert(&bs->solid_ot, pos, radius, blob_idx);
}

LiquidBlob *liquid_blob_create(BlobSim *bs) {
  LiquidBlob *b = fixed_array_append(&bs->liquids, NULL);
  if (!b) {
    fprintf(stderr, "Liquid blob max count reached\n");
    return NULL;
  }

  b->type = LIQUID_BASE;
  b->radius = BLOB_DEFAULT_RADIUS;
  b->pos = HMM_V3(0, 0, 0);
  b->mat_idx = 0;

  return b;
}

Projectile *projectile_create(BlobSim *bs) {
  Projectile *p = fixed_array_append(&bs->projectiles, NULL);
  if (!p) {
    fprintf(stderr, "Projectile max count reached\n");
    return NULL;
  }

  p->radius = BLOB_DEFAULT_RADIUS;
  p->pos = HMM_V3(0, 0, 0);
  p->mat_idx = 0;
  p->vel = HMM_V3(0, 0, 0);
  p->callback = NULL;
  p->userdata = 0;
  p->delete_timer = PROJECTILE_DEFAULT_DELETE_TIME;

  return p;
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
      blob_sim_queue_delete(bs, DELETE_COLLIDER_MODEL, cm);
      return;
    }
  }
}

void blob_sim_create(BlobSim *bs) {
  fixed_array_create(&bs->solids, sizeof(SolidBlob), BLOB_SIM_MAX_SOLIDS);
  fixed_array_create(&bs->liquids, sizeof(LiquidBlob), BLOB_SIM_MAX_LIQUIDS);
  fixed_array_create(&bs->projectiles, sizeof(Projectile),
                     BLOB_SIM_MAX_PROJECTILES);
  fixed_array_create(&bs->collider_models, sizeof(ColliderModel),
                     BLOB_SIM_MAX_COLLIDER_MODELS);

  for (int i = 0; i < DELETE_MAX; i++) {
    fixed_array_create(&bs->del_queues[i], sizeof(int), BLOB_SIM_MAX_DELETIONS);
  }

  bs->liq_forces = malloc(BLOB_SIM_MAX_FORCES * sizeof(*bs->liq_forces));
  for (int i = 0; i < BLOB_SIM_MAX_FORCES; i++) {
    bs->liq_forces[i].idx = -1;
  }

  bs->active_pos = HMM_V3(0, 0, 0);

  bs->solid_ot.max_subdiv = 4;
  bs->solid_ot.root_pos = HMM_V3(0, 0, 0);
  bs->solid_ot.root_size = BLOB_LEVEL_SIZE;
  blob_ot_create(&bs->solid_ot);
}

void blob_sim_destroy(BlobSim *bs) {
  fixed_array_destroy(&bs->solids);
  fixed_array_destroy(&bs->liquids);
  fixed_array_destroy(&bs->projectiles);
  fixed_array_destroy(&bs->collider_models);

  for (int i = 0; i < DELETE_MAX; i++) {
    fixed_array_destroy(&bs->del_queues[i]);
  }

  free(bs->liq_forces);
  bs->liq_forces = NULL;

  blob_ot_destroy(&bs->solid_ot);
}

void blob_sim_queue_delete(BlobSim *bs, DeletionType type, void *b) {
  int *idx = fixed_array_append(&bs->del_queues[type], NULL);
  if (!idx) {
    fprintf(stderr, "Too many blobs being deleted\n");
    return;
  }

  switch (type) {
  case DELETE_SOLID:
    *idx = fixed_array_get_idx_from_ptr(&bs->solids, b);
    break;
  case DELETE_LIQUID:
    *idx = fixed_array_get_idx_from_ptr(&bs->liquids, b);
    break;
  case DELETE_PROJECTILE:
    *idx = fixed_array_get_idx_from_ptr(&bs->projectiles, b);
    break;
  case DELETE_COLLIDER_MODEL:
    *idx = fixed_array_get_idx_from_ptr(&bs->collider_models, b);
    break;
  case DELETE_MAX:
    break;
  }
}

void blob_sim_liquid_apply_force(BlobSim *bs, const LiquidBlob *b,
                                 const HMM_Vec3 *force) {
  int blob_idx = fixed_array_get_idx_from_ptr(&bs->liquids, b);

  for (int i = 0; i < BLOB_SIM_MAX_FORCES; i++) {
    LiquidForce *f = &bs->liq_forces[i];
    if (f->idx == -1) {
      f->idx = blob_idx;
      f->force = *force;
      return;
    }
  }

  static bool printed_warning = false;
  if (!printed_warning) {
    printed_warning = true;
    fprintf(stderr, "Ran out of liquid forces\n");
  }
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

  for (int i = 0; i < BLOB_RAY_MAX_STEPS; i++) {
    HMM_Vec3 p = HMM_AddV3(ro, HMM_MulV3F(rd_n, traveled));

    float dist = 100000.0f;
    for (int j = 0; j < bs->solids.count; j++) {
      const SolidBlob *b = fixed_array_get_const(&bs->solids, j);
      dist = sminf(dist, dist_sphere(&b->pos, b->radius, &p), BLOB_SMOOTH);
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

void blob_simulate(BlobSim *bs, double delta) {
  // Liquid blobs
  for (int i = 0; i < bs->liquids.count; i++) {
    LiquidBlob *b = fixed_array_get(&bs->liquids, i);

    HMM_Vec3 velocity = {0};

    float anti_grav = 0.0f;

    for (int j = 0; j < bs->liquids.count; j++) {
      if (j == i)
        continue;

      LiquidBlob *ob = fixed_array_get(&bs->liquids, j);

      HMM_Vec3 attraction = blob_get_attraction_to(b, ob);
      velocity = HMM_AddV3(velocity, attraction);
      anti_grav += blob_get_support_with(b, ob);
    }

    velocity.Y -= LIQUID_FALL_SPEED * (1.0f - fminf(anti_grav, 1.0f));

    // Now position + velocity * delta will be the new position before dealing
    // with solid blobs

    b->pos = HMM_AddV3(b->pos, HMM_MulV3F(velocity, (float)delta));
    HMM_Vec3 correction =
        blob_get_correction_from_solids(bs, &b->pos, b->radius);
    b->pos = HMM_AddV3(b->pos, correction);

    // TODO: queue inactive
    /*
    for (int x = 0; x < 3; x++) {
      b->pos.Elements[x] = fmaxf(b->pos.Elements[x], BLOB_SIM_POS.Elements[x] -
                                                         BLOB_SIM_SIZE * 0.5f);
      b->pos.Elements[x] = fminf(b->pos.Elements[x], BLOB_SIM_POS.Elements[x] +
                                                         BLOB_SIM_SIZE * 0.5f);
    }
    */
  }

  // Liquid blob forces
  for (int i = 0; i < BLOB_SIM_MAX_FORCES; i++) {
    LiquidForce *f = &bs->liq_forces[i];
    if (f->idx >= 0) {
      HMM_Vec3 *p = &((LiquidBlob *)fixed_array_get(&bs->liquids, f->idx))->pos;
      *p = HMM_AddV3(*p, HMM_MulV3F(f->force, (float)delta));

      f->force = HMM_MulV3F(f->force, 1.0f - 0.9f * (float)delta);
      if (HMM_LenV3(f->force) < 0.1f) {
        f->idx = -1;
      }
    }
  }

  // Projectiles
  for (int i = 0; i < bs->projectiles.count; i++) {
    Projectile *p = fixed_array_get(&bs->projectiles, i);

    const HMM_Vec3 PROJ_GRAV = {0, -9.8f, 0};
    p->vel = HMM_AddV3(p->vel, HMM_MulV3F(PROJ_GRAV, (float)delta));
    p->pos = HMM_AddV3(p->pos, HMM_MulV3F(p->vel, (float)delta));

    HMM_Vec3 correction =
        blob_get_correction_from_solids(bs, &p->pos, p->radius);
    if (HMM_LenV3(correction) > 0.0f) {
      HMM_Vec3 n = HMM_NormV3(correction);
      HMM_Vec3 u = HMM_MulV3F(n, HMM_DotV3(p->vel, n));
      HMM_Vec3 w = HMM_SubV3(p->vel, u);
      const float f = 0.95f;
      const float r = 0.5f;
      p->vel = HMM_SubV3(HMM_MulV3F(w, f), HMM_MulV3F(u, r));
    }

    for (int c = 0; c < bs->collider_models.count; c++) {
      ColliderModel *col_mdl = fixed_array_get(&bs->collider_models, c);
      Model *mdl = entity_get_component_or_null(col_mdl->ent, COMPONENT_MODEL);
      if (!mdl)
        continue;

      for (int bi = 0; bi < mdl->blob_count; bi++) {
        ModelBlob *b = &mdl->blobs[bi];

        HMM_Vec4 bpv4 = {0, 0, 0, 1};
        bpv4.XYZ = b->pos;

        HMM_Mat4 *trans =
            entity_get_component(col_mdl->ent, COMPONENT_TRANSFORM);
        HMM_Vec3 bpos = HMM_MulM4V4(*trans, bpv4).XYZ;
        if (HMM_LenV3(HMM_SubV3(bpos, p->pos)) <= b->radius + p->radius) {
          if (p->callback) {
            p->callback(p, col_mdl, p->userdata);
          }
          break;
        }
      }
    }

    p->delete_timer -= (float)delta;
    if (p->delete_timer <= 0.0f) {
      blob_sim_queue_delete(bs, DELETE_PROJECTILE, p);
    }
  }

  // Deletion queues
  FixedArray *const blob_arrays[] = {&bs->solids, &bs->liquids,
                                     &bs->projectiles, &bs->collider_models};
  for (int bt = 0; bt < DELETE_MAX; bt++) {
    FixedArray *ba = blob_arrays[bt];
    FixedArray *del_queue = &bs->del_queues[bt];

    for (int di = 0; di < del_queue->count; di++) {
      int blob_idx = *(int *)fixed_array_get(del_queue, di);
      if (blob_idx == -1) {
        // This deletion was invalidated
        continue;
      }

      fixed_array_remove(ba, blob_idx);

      // Decrement blob indices in deletion queue after this one. Or invalidate
      // it if it is the same as the current index
      for (int dj = di + 1; dj < del_queue->count; dj++) {
        int *other_idx = fixed_array_get(del_queue, dj);
        if (*other_idx > blob_idx) {
          (*other_idx)--;
        } else if (*other_idx == blob_idx) {
          *other_idx = -1;
        }
      }

      /*
      if (bt == DELETE_COLLIDER_MODEL) {
        for (int i = 0; i < bs->collider_models.count; i++) {
          HMM_Vec3 trans =
              ((ColliderModel *)fixed_array_get(&bs->collider_models, i))
                  ->mdl->transform.Columns[3]
                  .XYZ;
          printf("(%f, %f, %f)\n", trans.X, trans.Y, trans.Z);
        }
      }
      */

      // Indices in liquid forces must be adjusted as well
      if (bt == DELETE_LIQUID) {
        for (int fi = 0; fi < BLOB_SIM_MAX_FORCES; fi++) {
          LiquidForce *f = &bs->liq_forces[fi];
          if (f->idx > blob_idx) {
            f->idx--;
          } else if (f->idx == blob_idx) {
            // If this liquid force applies to the current blob, stop it
            f->idx = -1;
          }
        }
      }
    }

    // Clear this queue
    del_queue->count = 0;
  }
}

void blob_mdl_create(Model *mdl, const ModelBlob *mdl_blob_src,
                     int mdl_blob_count) {
  mdl->blobs = malloc(sizeof(*mdl->blobs) * mdl_blob_count);
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
  bot->root = (BlobOtNode *)malloc(bot->capacity_int * sizeof(int));
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
static HMM_Vec3 ot_quadrants[8] = {{0.5f, 0.5f, 0.5f},   {0.5f, 0.5f, -0.5f},
                                   {0.5f, -0.5f, 0.5f},  {0.5f, -0.5f, -0.5f},
                                   {-0.5f, 0.5f, 0.5f},  {-0.5f, 0.5f, -0.5f},
                                   {-0.5f, -0.5f, 0.5f}, {-0.5f, -0.5f, -0.5f}};

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

  if (enum_data->curr_leaf_depth < BLOB_OT_MAX_SUBDIVISIONS &&
      leaf->leaf_blob_count == BLOB_OT_LEAF_SUBDIV_BLOB_COUNT) {
    int reinsert[BLOB_OT_LEAF_SUBDIV_BLOB_COUNT];
    for (int i = 0; i < BLOB_OT_LEAF_SUBDIV_BLOB_COUNT; i++) {
      reinsert[i] = leaf->offsets[i];
    }

    int old_node_size_int = 1 + BLOB_OT_LEAF_MAX_BLOB_COUNT;
    int new_node_size_int = 1 + 8;
    int children_size_int = (1 + BLOB_OT_LEAF_MAX_BLOB_COUNT) * 8;
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
      printf("Reallocating\n");
      int old_capacity = enum_data->bot->capacity_int;
      int new_capacity = old_capacity * 2;

      void *new_data = malloc(new_capacity * sizeof(int));
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

    for (int i = 0; i < 8; i++) {
      node->offsets[i] =
          new_node_size_int + i * (1 + BLOB_OT_LEAF_MAX_BLOB_COUNT);
      BlobOtNode *child = node + node->offsets[i];
      child->leaf_blob_count = 0;

      HMM_Vec3 child_pos = HMM_AddV3(
          HMM_MulV3F(ot_quadrants[i], enum_data->curr_leaf_size * 0.5f),
          *enum_data->curr_leaf_pos);
      float child_size = enum_data->curr_leaf_size * 0.5f;

      
      // TODO: Need a way to associate an index in the octree with a pos +
      // radius
      for (int j = 0; j < BLOB_OT_LEAF_SUBDIV_BLOB_COUNT; j++) {
        float dist = dist_cube(&child_pos, child_size, &enum_data->shape_pos) -
                     enum_data->shape_size;

        if (dist <= 0.0f) {
          // TODO: Need to account for if this newly created child also got filled
          if (child->leaf_blob_count < BLOB_OT_LEAF_SUBDIV_BLOB_COUNT - 1) {
            child->offsets[child->leaf_blob_count++] = reinsert[j];
          }
        }
      }
    }

    /*
    // Sanity check
    BlobOtNode *node_stack[BLOB_OT_MAX_SUBDIVISIONS + 2];
    node_stack[0] = enum_data->bot->root;
    int iter_stack[BLOB_OT_MAX_SUBDIVISIONS + 2];
    iter_stack[0] = 0;
    int depth = 0;
    while (depth > 0) {
      if (depth == BLOB_OT_MAX_SUBDIVISIONS) {
        printf("Epical!\n");
      }
      if (depth > BLOB_OT_MAX_SUBDIVISIONS) {
        printf("Erm what the sigma\n");
      }

      BlobOtNode *check_node = node_stack[depth];
      int *i = &iter_stack[depth];
      for (; *i < 8; (*i)++) {
        BlobOtNode *child = check_node + check_node->offsets[*i];
        if (child->leaf_blob_count == -1) {
          node_stack[depth + 1] = child;
          iter_stack[depth + 1] = 0;
          (*i)++;
          depth += 2;
          break;
        }
      }

      depth--;
    }
    */
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
      int *i = &iter_stack[node_depth];
      for (; *i < 8; (*i)++) {
        pos_stack[node_depth + 1] =
            HMM_AddV3(HMM_MulV3F(ot_quadrants[*i], nsize * 0.5f), *npos);
        size_stack[node_depth + 1] = nsize * 0.5f;

        float dist =
            dist_cube(&pos_stack[node_depth + 1], size_stack[node_depth + 1],
                      &enum_data->shape_pos) -
            enum_data->shape_size;

        if (dist <= 0.0f) {
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
            HMM_AddV3(HMM_MulV3F(ot_quadrants[*i], nsize * 0.5f), *npos);
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