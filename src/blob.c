#include "blob.h"

void blob_get_attraction_with(vec3 r, Blob* b, Blob* other) {
  vec3 direction;
  vec3_sub(direction, b->pos, other->pos);
  float len = vec3_len(direction);

  float x = fabsf(BLOB_DESIRED_DISTANCE - len);
  float power = fmaxf(0.0f, logf(-5.0f * x * (x - BLOB_DESIRED_DISTANCE) + 1.0f));
  //float power = fmaxf(0.0f, -2.1f * x * (x - BLOB_DESIRED_DISTANCE));

  vec3_norm(r, direction);
  vec3_scale(r, r, power);

  // Blobs only fall with gravity, they don't force each other to fall
  if (r[1] < 0.0f) {
    r[1] = 0.0f;
  }
}