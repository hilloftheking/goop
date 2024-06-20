#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "blob.h"

HMM_Vec3 blob_get_attraction_to(Blob *b, Blob *other) {
  HMM_Vec3 direction = HMM_SubV3(b->pos, other->pos);
  float len = HMM_LenV3(direction);

  float x = fabsf(BLOB_DESIRED_DISTANCE - len);

  if (x > BLOB_DESIRED_DISTANCE * 2.0f) {
    return (HMM_Vec3){0};
  }

  float power =
      fmaxf(0.0f, logf(-5.0f * x * (x - BLOB_DESIRED_DISTANCE) + 1.0f));
  // float power = fmaxf(0.0f, -2.1f * x * (x - BLOB_DESIRED_DISTANCE));

  direction = HMM_MulV3F(HMM_NormV3(direction), power);

  // Blobs only fall with gravity, they don't force each other to fall
  if (direction.Y < 0.0f) {
    direction.Y = 0.0f;
  }

  return direction;
}

float blob_get_support_with(Blob *b, Blob *other) {
  HMM_Vec3 direction = HMM_SubV3(b->pos, other->pos);
  float len = HMM_LenV3(direction);

  float x = BLOB_DESIRED_DISTANCE - len;

  if (fabsf(x) > BLOB_DESIRED_DISTANCE * 2.0f) {
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

bool blob_is_solid(Blob *b) { return b->type == BLOB_SOLID; }

Blob *blob_create(BlobSim *bs, BlobType type, float radius,
                  const HMM_Vec3 *pos, int mat_idx) {
  if (bs->blob_count >= BLOB_MAX_COUNT) {
    fprintf(stderr, "Blob max count reached\n");
    return NULL;
  }

  Blob *b = &bs->blobs[bs->blob_count];
  b->type = type;
  b->radius = radius;
  b->pos = *pos;
  b->prev_pos = *pos;
  b->mat_idx = mat_idx;

  if (type == BLOB_LIQUID) {
    b->liquid_sleep_ticks = 0;
  }

  bs->blob_count++;

  return b;
}

void blob_sim_create(BlobSim *bs) {
  bs->blob_count = 0;
  bs->blobs = malloc(BLOB_MAX_COUNT * sizeof(*bs->blobs));
  bs->tick_timer = 0.0;
}

void blob_sim_destroy(BlobSim *bs) {
  bs->blob_count = 0;
  free(bs->blobs);
  bs->blobs = NULL;

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

void blob_simulate(BlobSim *bs, double delta) {
  bs->tick_timer -= delta;

  if (bs->tick_timer > 0.0)
    return;
  bs->tick_timer = BLOB_TICK_TIME;

  for (int b = 0; b < bs->blob_count; b++) {
    if (blob_is_solid(&bs->blobs[b]))
      continue;
    if (bs->blobs[b].type == BLOB_LIQUID &&
        bs->blobs[b].liquid_sleep_ticks >= BLOB_SLEEP_TICKS_REQUIRED)
      continue;

    bs->blobs[b].prev_pos = bs->blobs[b].pos;

    HMM_Vec3 velocity = {0};

    if (bs->blobs[b].type == BLOB_LIQUID) {
      float anti_grav = 0.0f;

      for (int ob = 0; ob < bs->blob_count; ob++) {
        if (ob == b)
          continue;
        if (blob_is_solid(&bs->blobs[ob]))
          continue;

        HMM_Vec3 attraction =
            blob_get_attraction_to(&bs->blobs[b], &bs->blobs[ob]);

        float attraction_magnitude = HMM_LenV3(attraction);
        if (attraction_magnitude > BLOB_SLEEP_THRESHOLD)
          bs->blobs[ob].liquid_sleep_ticks = 0;

        velocity = HMM_AddV3(velocity, attraction);

        anti_grav += blob_get_support_with(&bs->blobs[b], &bs->blobs[ob]);
      }

      velocity.Y -= BLOB_FALL_SPEED * (1.0f - fminf(anti_grav, 1.0f));
    }

    // Now position + velocity will be the new position before dealing with
    // solid blobs

    HMM_Vec3 pos_old = bs->blobs[b].pos;

    bs->blobs[b].pos = HMM_AddV3(bs->blobs[b].pos, velocity);
    HMM_Vec3 correction = blob_get_correction_from_solids(bs, &bs->blobs[b].pos,
                                                          bs->blobs[b].radius);
    bs->blobs[b].pos = HMM_AddV3(bs->blobs[b].pos, correction);

    for (int x = 0; x < 3; x++) {
      bs->blobs[b].pos.Elements[x] =
          fmaxf(bs->blobs[b].pos.Elements[x],
                BLOB_SIM_POS.Elements[x] - BLOB_SIM_SIZE * 0.5f);
      bs->blobs[b].pos.Elements[x] =
          fminf(bs->blobs[b].pos.Elements[x],
                BLOB_SIM_POS.Elements[x] + BLOB_SIM_SIZE * 0.5f);
    }

#ifdef BLOB_SLEEP_ENABLED
    if (!blob_is_solid(&bs->blobs[b])) {
      // If movement during this tick was under the sleep threshold, the sleep
      // counter can be incremented
      vec3 pos_diff;
      vec3_sub(pos_diff, bs->blobs[b].pos, pos_old);
      float movement = vec3_len(pos_diff);
      if (movement < BLOB_SLEEP_THRESHOLD) {
        bs->blobs[b].liquid_sleep_ticks++;
      } else {
        bs->blobs[b].liquid_sleep_ticks = 0;
      }
    }
#endif
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

  for (int ob = 0; ob < bs->blob_count; ob++) {
    if (bs->blobs[ob].type == BLOB_LIQUID)
      continue;

    blob_check_blob_at(&min_dist, &correction, &bs->blobs[ob].pos,
                       bs->blobs[ob].radius, pos, radius, BLOB_SMOOTH);
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