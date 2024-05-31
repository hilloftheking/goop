#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "blob.h"

void blob_get_attraction_to(vec3 r, Blob* b, Blob* other) {
  vec3 direction;
  vec3_sub(direction, b->pos, other->pos);
  float len = vec3_len(direction);

  float x = fabsf(BLOB_DESIRED_DISTANCE - len);

  if (x > BLOB_DESIRED_DISTANCE * 2.0f) {
    vec3_scale(r, r, 0.0f);
    return;
  }

  float power = fmaxf(0.0f, logf(-5.0f * x * (x - BLOB_DESIRED_DISTANCE) + 1.0f));
  //float power = fmaxf(0.0f, -2.1f * x * (x - BLOB_DESIRED_DISTANCE));

  vec3_norm(r, direction);
  vec3_scale(r, r, power);

  // Blobs only fall with gravity, they don't force each other to fall
  if (r[1] < 0.0f) {
    r[1] = 0.0f;
  }
}

float blob_get_support_with(Blob* b, Blob* other) {
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

void simulate_blobs(Blob* blobs, int amount, vec3 *blobs_prev_pos) {
  for (int b = 0; b < amount; b++) {
    if (blobs[b].sleep_ticks >= BLOB_SLEEP_TICKS_REQUIRED) {
      continue;
    }

    vec3_dup(blobs_prev_pos[b], blobs[b].pos);

    vec3 velocity = {0};
    float attraction_total_magnitude = 0.0f;
    float anti_grav = 0.0f;

    for (int ob = 0; ob < amount; ob++) {
      if (ob == b)
        continue;

      vec3 attraction;
      blob_get_attraction_to(attraction, &blobs[b], &blobs[ob]);

      float attraction_magnitude = vec3_len(attraction);
      if (attraction_magnitude > BLOB_SLEEP_THRESHOLD) {
        blobs[ob].sleep_ticks = 0;
      }

      attraction_total_magnitude += vec3_len(attraction);
      vec3_add(velocity, velocity, attraction);

      anti_grav += blob_get_support_with(&blobs[b], &blobs[ob]);
    }

    velocity[1] -= BLOB_FALL_SPEED * (1.0f - fminf(anti_grav, 1.0f));

    vec3 pos_before;
    vec3_dup(pos_before, blobs[b].pos);

    vec3_add(blobs[b].pos, blobs[b].pos, velocity);

    vec3_max(blobs[b].pos, blobs[b].pos, blob_min_pos);
    vec3_min(blobs[b].pos, blobs[b].pos, blob_max_pos);

    // If movement during this tick was under the sleep threshold, the sleep
    // counter can be incremented
    vec3 pos_diff;
    vec3_sub(pos_diff, blobs[b].pos, pos_before);
#ifdef BLOB_SLEEP_ENABLED
    float movement = vec3_len(pos_diff);
    if (movement < BLOB_SLEEP_THRESHOLD) {
      blobs[b].sleep_ticks++;
    } else {
      blobs[b].sleep_ticks = 0;
    }
#endif
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
  BlobOtNode *root = blob_ot + 0;
  root->leaf_blob_count = -1;

  int current_idx = 9;
  for (int j = 0; j < 8; j++) {
    BlobOtNode *node = blob_ot + current_idx;
    node->leaf_blob_count = 0;
    root->indices[j] = current_idx;
    current_idx += 1 + BLOB_OT_LEAF_MAX_BLOB_COUNT;
  }
}

BlobOt blob_ot_create() {
  int alloc_size = blob_ot_get_alloc_size();
  BlobOt blob_ot = malloc(alloc_size);

  setup_test_octree(blob_ot);

  return blob_ot;
}

void blob_ot_reset(BlobOt blob_ot) {
  setup_test_octree(blob_ot);
}

static vec3 ot_quadrants[8] = {{0.5f, 0.5f, 0.5f},   {0.5f, 0.5f, -0.5f},
                               {0.5f, -0.5f, 0.5f},  {0.5f, -0.5f, -0.5f},
                               {-0.5f, 0.5f, 0.5f},  {-0.5f, 0.5f, -0.5f},
                               {-0.5f, -0.5f, 0.5f}, {-0.5f, -0.5f, -0.5f}};

static float dist_cube(vec3 c, float s, vec3 p) {
  float max_dist = fabsf(p[0] - c[0]) - s;
  for (int i = 0; i < 3; i++) {
    float dist = fabsf(p[i] - c[i]) - s;
    if (dist > max_dist) {
      max_dist = dist;
    }
  }

  return max_dist;
}

static void insert_into_nodes(BlobOt blob_ot, BlobOtNode *node, vec3 node_pos,
                              vec3 node_size, vec3 blob_pos, int blob_idx) {
  if (node->leaf_blob_count == -1) {
    // This node has child nodes
    for (int i = 0; i < 8; i++) {
      vec3 child_pos;
      vec3_mul(child_pos, node_size, ot_quadrants[i]);
      vec3_add(child_pos, node_pos, child_pos);
      vec3 child_size;
      vec3_scale(child_size, node_size, 0.5f);

      float dist = dist_cube(child_pos, child_size[0], blob_pos) - BLOB_RADIUS -
                   BLOB_SMOOTH - BLOB_SDF_MAX_DIST;

      if (dist <= 0.0f) {
        insert_into_nodes(blob_ot, blob_ot + node->indices[i], child_pos,
                          child_size, blob_pos, blob_idx);
      }
    }
  } else {
    // This is a leaf node, but we already know we should go in it
    if (node->leaf_blob_count >= BLOB_OT_LEAF_MAX_BLOB_COUNT) {
      //fprintf(stderr, "Leaf node is full!\n");
      return;
    }

    node->indices[node->leaf_blob_count++] = blob_idx;
  }
}

void blob_ot_insert(BlobOt blob_ot, vec3 blob_pos, int blob_idx) {
  BlobOtNode *root = blob_ot + 0;
  vec3 root_pos = {0.0f, 8.0f, 0.0f};
  vec3 root_size = {BLOB_SDF_SIZE};
  insert_into_nodes(blob_ot, root, root_pos, root_size, blob_pos, blob_idx);
}