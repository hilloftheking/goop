#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
      fmaxf(0.0f, logf(-50.0f * x * (x - desired_dist) + 1.0f));

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
  SolidBlob *b = fixed_array_append(&bs->sol_blobs, NULL);
  if (!b) {
    fprintf(stderr, "Solid blob max count reached\n");
    return NULL;
  }

  b->radius = radius;
  b->pos = *pos;
  b->mat_idx = mat_idx;

  return b;
}

LiquidBlob *liquid_blob_create(BlobSim *bs, LiquidType type, float radius,
                               const HMM_Vec3 *pos, int mat_idx) {
  LiquidBlob *b = fixed_array_append(&bs->liq_blobs, NULL);
  if (!b) {
    fprintf(stderr, "Liquid blob max count reached\n");
    return NULL;
  }

  b->type = type;
  b->radius = radius;
  b->pos = *pos;
  b->mat_idx = mat_idx;

  return b;
}

Projectile *projectile_create(BlobSim *bs, float radius, const HMM_Vec3 *pos,
                              int mat_idx, const HMM_Vec3 *vel) {
  Projectile *p = fixed_array_append(&bs->projectiles, NULL);
  if (!p) {
    fprintf(stderr, "Projectile max count reached\n");
    return NULL;
  }

  p->radius = radius;
  p->pos = *pos;
  p->mat_idx = mat_idx;
  p->vel = *vel;
  p->callback = NULL;
  p->userdata = 0;

  return p;
}

void liquid_blob_queue_delete(BlobSim *bs, LiquidBlob *b) {
  int *idx = fixed_array_append(&bs->liq_blob_del_queue, NULL);
  if (!idx) {
    fprintf(stderr, "Too many blobs being deleted\n");
    return;
  }

  *idx = b - (LiquidBlob *)bs->liq_blobs.data;
}

void blob_sim_create(BlobSim *bs) {
  fixed_array_create(&bs->sol_blobs, sizeof(SolidBlob), SOLID_BLOB_MAX_COUNT);
  fixed_array_create(&bs->liq_blobs, sizeof(LiquidBlob), LIQUID_BLOB_MAX_COUNT);
  fixed_array_create(&bs->projectiles, sizeof(Projectile),
                     PROJECTILE_MAX_COUNT);
  fixed_array_create(&bs->liq_blob_del_queue, sizeof(int),
                     BLOB_SIM_MAX_DELETIONS);

  bs->liq_forces = malloc(BLOB_SIM_MAX_FORCES * sizeof(*bs->liq_forces));
  for (int i = 0; i < BLOB_SIM_MAX_FORCES; i++) {
    bs->liq_forces[i].idx = -1;
  }
}

void blob_sim_destroy(BlobSim *bs) {
  fixed_array_destroy(&bs->sol_blobs);
  fixed_array_destroy(&bs->liq_blobs);
  fixed_array_destroy(&bs->projectiles);
  fixed_array_destroy(&bs->liq_blob_del_queue);

  free(bs->liq_forces);
  bs->liq_forces = NULL;
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
    for (int j = 0; j < bs->sol_blobs.count; j++) {
      const SolidBlob *b = fixed_array_get_const(&bs->sol_blobs, j);
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
  for (int i = 0; i < bs->liq_blobs.count; i++) {
    LiquidBlob *b = fixed_array_get(&bs->liq_blobs, i);

    HMM_Vec3 velocity = {0};

    float anti_grav = 0.0f;

    for (int j = 0; j < bs->liq_blobs.count; j++) {
      if (j == i)
        continue;

      LiquidBlob *ob = fixed_array_get(&bs->liq_blobs, j);

      HMM_Vec3 attraction = blob_get_attraction_to(b, ob);
      velocity = HMM_AddV3(velocity, attraction);
      anti_grav += blob_get_support_with(b, ob);
    }

    velocity.Y -= BLOB_FALL_SPEED * (1.0f - fminf(anti_grav, 1.0f));

    // Now position + velocity * delta will be the new position before dealing with
    // solid blobs

    b->pos = HMM_AddV3(b->pos, HMM_MulV3F(velocity, (float)delta));
    HMM_Vec3 correction =
        blob_get_correction_from_solids(bs, &b->pos, b->radius);
    b->pos = HMM_AddV3(b->pos, correction);

    for (int x = 0; x < 3; x++) {
      b->pos.Elements[x] = fmaxf(b->pos.Elements[x], BLOB_SIM_POS.Elements[x] -
                                                         BLOB_SIM_SIZE * 0.5f);
      b->pos.Elements[x] = fminf(b->pos.Elements[x], BLOB_SIM_POS.Elements[x] +
                                                         BLOB_SIM_SIZE * 0.5f);
    }
  }

  // Liquid blob forces
  for (int i = 0; i < BLOB_SIM_MAX_FORCES; i++) {
    LiquidForce *f = &bs->liq_forces[i];
    if (f->idx >= 0) {
      HMM_Vec3 *p =
          &((LiquidBlob *)fixed_array_get(&bs->liq_blobs, f->idx))->pos;
      *p = HMM_AddV3(*p, f->force);

      f->force = HMM_MulV3F(f->force, 0.8f);
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

    if (p->callback)
      p->callback(p, p->userdata);
  }

  // Deletion queue
  for (int di = 0; di < bs->liq_blob_del_queue.count; di++) {
    // Move everything after this blob to the left
    int blob_idx = *(int *)fixed_array_get(&bs->liq_blob_del_queue, di);
    fixed_array_remove(&bs->liq_blobs, blob_idx);

    // Decrement blob indices in liquid forces
    for (int fi = 0; fi < BLOB_SIM_MAX_FORCES; fi++) {
      LiquidForce *f = &bs->liq_forces[fi];
      if (f->idx == blob_idx) {
        // If this liquid force applies to the current blob, stop it
        f->idx = -1;
      } else if (f->idx > blob_idx) {
        f->idx--;
      }
    }

    // Also decrement blob indices in the deletion queue
    for (int dj = di + 1; dj < bs->liq_blob_del_queue.count; dj++) {
      int *other_idx = fixed_array_get(&bs->liq_blob_del_queue, dj);
      if (*other_idx > blob_idx) {
        (*other_idx)--;
      }
    }
  }
  // Clear deletion queue
  bs->liq_blob_del_queue.count = 0;
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

  for (int i = 0; i < bs->sol_blobs.count; i++) {
    SolidBlob *b = fixed_array_get(&bs->sol_blobs, i);
    blob_check_blob_at(&min_dist, &correction, &b->pos, b->radius, pos, radius,
                       BLOB_SMOOTH);
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