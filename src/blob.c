#include <math.h>

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
  for (int b = 0; b < BLOB_COUNT; b++) {
    if (blobs[b].sleep_ticks >= BLOB_SLEEP_TICKS_REQUIRED) {
      continue;
    }

    vec3_dup(blobs_prev_pos[b], blobs[b].pos);

    vec3 velocity = {0};
    float attraction_total_magnitude = 0.0f;
    float anti_grav = 0.0f;

    for (int ob = 0; ob < BLOB_COUNT; ob++) {
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

    for (int i = 0; i < 3; i++) {
      if (blobs[b].pos[i] < blob_min_pos[i]) {
        blobs[b].pos[i] = blob_min_pos[i];
      }
      if (blobs[b].pos[i] > blob_max_pos[i]) {
        blobs[b].pos[i] = blob_max_pos[i];
      }
    }

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