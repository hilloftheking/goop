#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "blob.h"

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
      fmaxf(0.0f, logf(-5.0f * x * (x - desired_dist) + 1.0f));
  // float power = fmaxf(0.0f, -2.1f * x * (x - desired_dist));

  direction = HMM_MulV3F(HMM_NormV3(direction), power);

  // Blobs only fall with gravity, they don't force each other to fall
  if (direction.Y < 0.0f) {
    direction.Y = 0.0f;
  }

  return direction;
}

float blob_get_support_with(LiquidBlob *b, LiquidBlob *other) {
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

  return fmaxf(0.0f, -1.0f * x * x + 0.4f);
}

SolidBlob *solid_blob_create(BlobSim *bs, float radius, const HMM_Vec3 *pos,
                             int mat_idx) {
  if (bs->sol_blob_count >= SOLID_BLOB_MAX_COUNT) {
    fprintf(stderr, "Solid blob max count reached\n");
    return NULL;
  }

  SolidBlob *b = &bs->sol_blobs[bs->sol_blob_count];
  b->radius = radius;
  b->pos = *pos;
  b->mat_idx = mat_idx;

  bs->sol_blob_count++;

  return b;
}

LiquidBlob *liquid_blob_create(BlobSim *bs, LiquidType type, float radius,
                               const HMM_Vec3 *pos, int mat_idx) {
  if (bs->liq_blob_count >= LIQUID_BLOB_MAX_COUNT) {
    fprintf(stderr, "Liquid blob max count reached\n");
    return NULL;
  }

  LiquidBlob *b = &bs->liq_blobs[bs->liq_blob_count];
  b->type = type;
  b->radius = radius;
  b->pos = *pos;
  b->prev_pos = *pos;
  b->mat_idx = mat_idx;

  bs->liq_blob_count++;

  return b;
}

void blob_sim_create(BlobSim *bs) {
  bs->sol_blob_count = 0;
  bs->sol_blobs = malloc(SOLID_BLOB_MAX_COUNT * sizeof(*bs->sol_blobs));

  bs->liq_blob_count = 0;
  bs->liq_blobs = malloc(LIQUID_BLOB_MAX_COUNT * sizeof(*bs->liq_blobs));

  bs->liq_forces = malloc(BLOB_SIM_MAX_FORCES * sizeof(*bs->liq_forces));
  for (int i = 0; i < BLOB_SIM_MAX_FORCES; i++) {
    bs->liq_forces[i].idx = -1;
  }

  bs->tick_timer = 0.0;
}

void blob_sim_destroy(BlobSim *bs) {
  bs->sol_blob_count = 0;
  free(bs->sol_blobs);
  bs->sol_blobs = NULL;

  bs->liq_blob_count = 0;
  free(bs->liq_blobs);
  bs->liq_blobs = NULL;

  free(bs->liq_forces);
  bs->liq_forces = NULL;

  bs->tick_timer = 0.0;
}

void blob_sim_create_mdl(Model *mdl, BlobSim *bs, const ModelBlob *mdl_blob_src,
                         int mdl_blob_count) {
  mdl->blobs = malloc(sizeof(*mdl->blobs) * mdl_blob_count);
  mdl->blob_count = mdl_blob_count;
  mdl->transform = HMM_M4D(1.0f);

  for (int i = 0; i < mdl->blob_count; i++) {
    mdl->blobs[i] = mdl_blob_src[i];
  }
}

void blob_sim_add_force(BlobSim *bs, int blob_idx, const HMM_Vec3 *force) {
  for (int i = 0; i < BLOB_SIM_MAX_FORCES; i++) {
    LiquidForce *f = &bs->liq_forces[i];
    if (f->idx == -1) {
      f->idx = blob_idx;
      f->force = *force;
      return;
    }
  }

  fprintf(stderr, "Ran out of liquid forces\n");
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

void blob_sim_raycast(RaycastResult *r, const BlobSim *bs, HMM_Vec3 ro, HMM_Vec3 rd) {
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
    for (int b = 0; b < bs->sol_blob_count; b++) {
      dist = sminf(
          dist, dist_sphere(&bs->sol_blobs[b].pos, bs->sol_blobs[b].radius, &p),
          BLOB_SMOOTH);
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
  bs->tick_timer -= delta;

  if (bs->tick_timer > 0.0)
    return;
  bs->tick_timer = BLOB_TICK_TIME;

  for (int i = 0; i < bs->liq_blob_count; i++) {
    LiquidBlob *b = &bs->liq_blobs[i];

    b->prev_pos = b->pos;

    HMM_Vec3 velocity = {0};

    float anti_grav = 0.0f;

    for (int j = 0; j < bs->liq_blob_count; j++) {
      if (j == i)
        continue;

      LiquidBlob *ob = &bs->liq_blobs[j];

      HMM_Vec3 attraction = blob_get_attraction_to(b, ob);
      velocity = HMM_AddV3(velocity, attraction);
      anti_grav += blob_get_support_with(b, ob);
    }

    velocity.Y -= BLOB_FALL_SPEED * (1.0f - fminf(anti_grav, 1.0f));

    // Now position + velocity will be the new position before dealing with
    // solid blobs

    b->pos = HMM_AddV3(b->pos, velocity);
    HMM_Vec3 correction =
        blob_get_correction_from_solids(bs, &b->pos, b->radius);
    b->pos = HMM_AddV3(b->pos, correction);

    for (int x = 0; x < 3; x++) {
      b->pos.Elements[x] = fmaxf(b->pos.Elements[x], BLOB_SIM_POS.Elements[x] -
                                                         BLOB_SIM_SIZE * 0.5f);
      b->pos.Elements[x] = fminf(b->pos.Elements[x], BLOB_SIM_POS.Elements[x] +
                                                         BLOB_SIM_SIZE * 0.5f);
    }

    if (b->type == LIQUID_PROJECTILE) {
      b->projectile.callback(b, b->projectile.userdata);
    }
  }

  for (int i = 0; i < BLOB_SIM_MAX_FORCES; i++) {
    LiquidForce *f = &bs->liq_forces[i];
    if (f->idx >= 0) {
      HMM_Vec3 *p = &bs->liq_blobs[f->idx].pos;
      *p = HMM_AddV3(*p, f->force);

      f->force = HMM_MulV3F(f->force, 0.8f);
      if (HMM_LenV3(f->force) < 0.1f) {
        f->idx = -1;
      }
    }
  }
}

static void blob_check_blob_at(float *min_dist, HMM_Vec3 *correction,
                               const HMM_Vec3 *bpos, float bradius, const HMM_Vec3 *pos,
                               float radius, float smooth) {
  *min_dist = sminf(*min_dist, dist_sphere(bpos, bradius, pos), smooth);

  HMM_Vec3 dir = HMM_SubV3(*pos, *bpos);
  float influence = fmaxf(0.0f, radius + bradius + smooth - HMM_LenV3(dir));
  *correction = HMM_AddV3(*correction, HMM_MulV3F(HMM_NormV3(dir), influence));
}

HMM_Vec3 blob_get_correction_from_solids(BlobSim *bs, const HMM_Vec3 *pos,
                                         float radius) {
  HMM_Vec3 correction = {0};

  float min_dist = 10000.0f;

  // Use smin to figure out if this blob is touching smoothed solid blobs
  // Also check how much each blob should influence the normal

  for (int i = 0; i < bs->sol_blob_count; i++) {
    blob_check_blob_at(&min_dist, &correction, &bs->sol_blobs[i].pos,
                       bs->sol_blobs[i].radius, pos, radius, BLOB_SMOOTH);
  }

  // Is this blob inside of a solid blob?
  if (min_dist < radius) {
    return HMM_MulV3F(HMM_NormV3(correction), radius - min_dist);
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

static void setup_test_octree(BlobOt blob_ot) {
  BlobOtNode *first = blob_ot + 0;
  first->leaf_blob_count = -1;

  int current_idx = 9;
  for (int i = 0; i < 8; i++) {
    BlobOtNode *second = blob_ot + current_idx;
    second->leaf_blob_count = -1;
    first->indices[i] = current_idx;
    current_idx += 9;

    for (int j = 0; j < 8; j++) {
      BlobOtNode *third = blob_ot + current_idx;
      third->leaf_blob_count = -1;
      second->indices[j] = current_idx;
      current_idx += 9;

      for (int k = 0; k < 8; k++) {
        BlobOtNode *leaf = blob_ot + current_idx;
        leaf->leaf_blob_count = 0;
        third->indices[k] = current_idx;
        current_idx += 1 + BLOB_OT_LEAF_MAX_BLOB_COUNT;
      }
    }
  }
}

BlobOt blob_ot_create() {
  int alloc_size = blob_ot_get_alloc_size();
  BlobOt blob_ot = malloc(alloc_size);

  setup_test_octree(blob_ot);

  return blob_ot;
}

void blob_ot_reset(BlobOt blob_ot) { setup_test_octree(blob_ot); }

static HMM_Vec3 ot_quadrants[8] = {{0.5f, 0.5f, 0.5f},   {0.5f, 0.5f, -0.5f},
                                   {0.5f, -0.5f, 0.5f},  {0.5f, -0.5f, -0.5f},
                                   {-0.5f, 0.5f, 0.5f},  {-0.5f, 0.5f, -0.5f},
                                   {-0.5f, -0.5f, 0.5f}, {-0.5f, -0.5f, -0.5f}};

static float dist_cube(const HMM_Vec3 *c, float s, const HMM_Vec3 *p) {
  HMM_Vec3 d;
  for (int i = 0; i < 3; i++) {
    d.Elements[i] =
        fmaxf(0.0f, fabsf(p->Elements[i] - c->Elements[i]) - s * 0.5f);
  }
  return HMM_LenV3(d);
}

static void insert_into_nodes(BlobOt blob_ot, BlobOtNode *node,
                              const HMM_Vec3 *node_pos, float node_size,
                              const HMM_Vec3 *blob_pos, int blob_idx) {
  if (node->leaf_blob_count == -1) {
    // This node has child nodes
    for (int i = 0; i < 8; i++) {
      HMM_Vec3 child_pos =
          HMM_AddV3(HMM_MulV3F(ot_quadrants[i], node_size * 0.5f), *node_pos);
      float child_size = node_size * 0.5f;

      float dist = dist_cube(&child_pos, child_size, blob_pos) -
                   BLOB_MAX_RADIUS - BLOB_SMOOTH - BLOB_SDF_MAX_DIST;

      if (dist <= 0.0f) {
        insert_into_nodes(blob_ot, blob_ot + node->indices[i], &child_pos,
                          child_size, blob_pos, blob_idx);
      }
    }
  } else {
    // This is a leaf node, but we already know we should go in it
    if (node->leaf_blob_count >= BLOB_OT_LEAF_MAX_BLOB_COUNT) {
      static bool printed_warning = false;
      if (!printed_warning) {
        printed_warning = true;
        fprintf(stderr, "Leaf node is full!\n");
      }
      return;
    }

    node->indices[node->leaf_blob_count++] = blob_idx;
  }
}

void blob_ot_insert(BlobOt blob_ot, const HMM_Vec3 *blob_pos, int blob_idx) {
  BlobOtNode *root = blob_ot + 0;
  insert_into_nodes(blob_ot, root, &BLOB_SIM_POS, BLOB_SIM_SIZE, blob_pos,
                    blob_idx);
}