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

  if (x > 0.0f) {
    x *= 0.1f;
  }
  return fmaxf(0.0f, -1.0f * x * x + 0.33f);
}