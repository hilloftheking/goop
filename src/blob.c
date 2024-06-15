#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "blob.h"

void blob_get_attraction_to(vec3 r, Blob *b, Blob *other) {
  vec3 direction;
  vec3_sub(direction, b->pos, other->pos);
  float len = vec3_len(direction);

  float x = fabsf(BLOB_DESIRED_DISTANCE - len);

  if (x > BLOB_DESIRED_DISTANCE * 2.0f) {
    vec3_scale(r, r, 0.0f);
    return;
  }

  float power =
      fmaxf(0.0f, logf(-5.0f * x * (x - BLOB_DESIRED_DISTANCE) + 1.0f));
  // float power = fmaxf(0.0f, -2.1f * x * (x - BLOB_DESIRED_DISTANCE));

  vec3_norm(r, direction);
  vec3_scale(r, r, power);

  // Blobs only fall with gravity, they don't force each other to fall
  if (r[1] < 0.0f) {
    r[1] = 0.0f;
  }
}

float blob_get_support_with(Blob *b, Blob *other) {
  vec3 direction;
  vec3_sub(direction, b->pos, other->pos);
  float len = vec3_len(direction);

  float x = BLOB_DESIRED_DISTANCE - len;

  if (fabsf(x) > BLOB_DESIRED_DISTANCE * 2.0f) {
    return 0.0f;
  }

  // More support when close together
  if (x > 0.0f) {
    x *= 0.1f;
  }

  // Less support when other blob is above
  if (direction[1] <= 0.1f) {
    x *= 1.4f;
  }

  return fmaxf(0.0f, -1.0f * x * x + 0.4f);
}

bool blob_is_solid(Blob *b) { return b->type == BLOB_SOLID; }

Blob *blob_create(BlobSimulation *bs, BlobType type, float radius,
                  const vec3 pos, int mat_idx) {
  if (bs->blob_count >= BLOB_MAX_COUNT) {
    fprintf(stderr, "Blob max count reached\n");
    return NULL;
  }

  Blob *b = &bs->blobs[bs->blob_count];
  b->type = type;
  b->radius = radius;
  vec3_dup(b->pos, pos);
  vec3_dup(b->prev_pos, pos);
  b->mat_idx = mat_idx;

  if (type == BLOB_LIQUID) {
    b->liquid_sleep_ticks = 0;
  }

  bs->blob_count++;

  return b;
}

BlobChar* blob_char_create(BlobSimulation* bs, float radius, const vec3 pos,
    int mat_idx) {
  if (bs->blob_char_count >= BLOB_CHAR_MAX_COUNT) {
    fprintf(stderr, "Blob character max count reached\n");
    return NULL;
  }

  BlobChar *b = &bs->blob_chars[bs->blob_char_count];
  b->radius = radius;
  vec3_dup(b->pos, pos);
  b->mat_idx = mat_idx;

  bs->blob_char_count++;

  return b;
}

static float rand_float() { return ((float)rand() / (float)(RAND_MAX)); }

void blob_simulation_create(BlobSimulation *bs) {
  bs->blob_count = 0;
  bs->blobs = malloc(BLOB_MAX_COUNT * sizeof(*bs->blobs));
  bs->blob_char_count = 0;
  bs->blob_chars = malloc(BLOB_CHAR_MAX_COUNT * sizeof(*bs->blob_chars));
  bs->tick_timer = 0.0;

  for (int i = 0; i < BLOB_START_COUNT; i++) {
    vec3 pos = {(rand_float() - 0.5f) * 5.0f, 4.0f + (rand_float() * 6.0f),
                (rand_float() - 0.5f) * 5.0f};
    blob_create(bs, i % 2 /* alternate between solid and liquid */, 0.5f, pos,
                i % 2);
  }
}

void blob_simulation_destroy(BlobSimulation *bs) {
  bs->blob_count = 0;
  free(bs->blobs);
  bs->blobs = NULL;

  bs->blob_char_count = 0;
  free(bs->blob_chars);
  bs->blob_chars = NULL;

  bs->tick_timer = 0.0;
}

void blob_simulate(BlobSimulation *bs, double delta) {
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

    vec3_dup(bs->blobs[b].prev_pos, bs->blobs[b].pos);

    vec3 velocity = {0};

    if (bs->blobs[b].type == BLOB_LIQUID) {
      float anti_grav = 0.0f;

      for (int ob = 0; ob < bs->blob_count; ob++) {
        if (ob == b)
          continue;
        if (blob_is_solid(&bs->blobs[ob]))
          continue;

        vec3 attraction;
        blob_get_attraction_to(attraction, &bs->blobs[b], &bs->blobs[ob]);

        float attraction_magnitude = vec3_len(attraction);
        if (attraction_magnitude > BLOB_SLEEP_THRESHOLD)
          bs->blobs[ob].liquid_sleep_ticks = 0;

        vec3_add(velocity, velocity, attraction);

        anti_grav += blob_get_support_with(&bs->blobs[b], &bs->blobs[ob]);
      }

      velocity[1] -= BLOB_FALL_SPEED * (1.0f - fminf(anti_grav, 1.0f));
    }

    // Now position + velocity will be the new position before dealing with
    // solid blobs

    vec3 pos_old;
    vec3_dup(pos_old, bs->blobs[b].pos);

    vec3_add(bs->blobs[b].pos, bs->blobs[b].pos, velocity);

    vec3 correction;
    blob_get_correction_from_solids(correction, bs, bs->blobs[b].pos,
                                    bs->blobs[b].radius, true);
    vec3_add(bs->blobs[b].pos, bs->blobs[b].pos, correction);

    vec3_max(bs->blobs[b].pos, bs->blobs[b].pos, blob_min_pos);
    vec3_min(bs->blobs[b].pos, bs->blobs[b].pos, blob_max_pos);

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

static float dist_sphere(const vec3 c, float r, const vec3 p) {
  vec3 d;
  vec3_sub(d, p, c);
  return vec3_len(d) - r;
}

static void blob_check_blob_at(float *min_dist, vec3 correction,
                               const vec3 bpos, float bradius, const vec3 pos,
                               float radius) {
  *min_dist = sminf(*min_dist, dist_sphere(bpos, bradius, pos), BLOB_SMOOTH);

  vec3 dir;
  vec3_sub(dir, pos, bpos);
  float influence = fmaxf(0.0f, radius + bradius + BLOB_SMOOTH - vec3_len(dir));
  vec3_norm(dir, dir);
  vec3_scale(dir, dir, influence);
  vec3_add(correction, correction, dir);
}

void blob_get_correction_from_solids(vec3 correction, BlobSimulation *bs,
                                     const vec3 pos, float radius, bool check_chars) {
  vec3_dup(correction, (vec3){0});

  float min_dist = 10000.0f;

  // Use smin to figure out if this blob is touching smoothed solid blobs
  // Also check how much each blob should influence the normal

  for (int ob = 0; ob < bs->blob_count; ob++) {
    if (bs->blobs[ob].type == BLOB_LIQUID)
      continue;

    blob_check_blob_at(&min_dist, correction, bs->blobs[ob].pos,
                       bs->blobs[ob].radius, pos, radius);
  }

  if (check_chars) {
    for (int ob = 0; ob < bs->blob_char_count; ob++) {
      blob_check_blob_at(&min_dist, correction, bs->blob_chars[ob].pos,
                         bs->blob_chars[ob].radius, pos, radius);
    }
  }

  // Is this blob inside of a solid blob?
  if (min_dist < radius) {
    vec3_norm(correction, correction);
    vec3_scale(correction, correction, radius - min_dist);
  } else {
    vec3_dup(correction, (vec3){0});
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

static vec3 ot_quadrants[8] = {{0.5f, 0.5f, 0.5f},   {0.5f, 0.5f, -0.5f},
                               {0.5f, -0.5f, 0.5f},  {0.5f, -0.5f, -0.5f},
                               {-0.5f, 0.5f, 0.5f},  {-0.5f, 0.5f, -0.5f},
                               {-0.5f, -0.5f, 0.5f}, {-0.5f, -0.5f, -0.5f}};

static float dist_cube(vec3 c, float s, vec3 p) {
  vec3 d;
  for (int i = 0; i < 3; i++) {
    d[i] = fmaxf(0.0f, fabsf(p[i] - c[i]) - s * 0.5f);
  }
  return vec3_len(d);
}

static void insert_into_nodes(BlobOt blob_ot, BlobOtNode *node, vec3 node_pos,
                              vec3 node_size, vec3 blob_pos, int blob_idx) {
  if (node->leaf_blob_count == -1) {
    // This node has child nodes
    for (int i = 0; i < 8; i++) {
      vec3 child_pos;
      vec3_mul(child_pos, node_size, ot_quadrants[i]);
      vec3_scale(child_pos, child_pos, 0.5f);
      vec3_add(child_pos, node_pos, child_pos);
      vec3 child_size;
      vec3_scale(child_size, node_size, 0.5f);

      float dist = dist_cube(child_pos, child_size[0], blob_pos) -
                   BLOB_MAX_RADIUS - BLOB_SMOOTH - BLOB_SDF_MAX_DIST;

      if (dist <= 0.0f) {
        insert_into_nodes(blob_ot, blob_ot + node->indices[i], child_pos,
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

void blob_ot_insert(BlobOt blob_ot, vec3 blob_pos, int blob_idx) {
  BlobOtNode *root = blob_ot + 0;
  vec3 root_pos = {BLOB_SDF_SIZE};
  vec3_scale(root_pos, root_pos, 0.5f);
  vec3_add(root_pos, root_pos, blob_sdf_start);
  vec3 root_size = {BLOB_SDF_SIZE};
  insert_into_nodes(blob_ot, root, root_pos, root_size, blob_pos, blob_idx);
}