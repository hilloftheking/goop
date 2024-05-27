#include "blob.h"

void blob_get_attraction_with(vec3 r, Blob* b, Blob* other) {
  vec3 direction;
  vec3_sub(direction, b->pos, other->pos);
  float len = vec3_len(direction);

  // -10(x+0.4)(x^2)(x-0.4)
  float x = (BLOB_DESIRED_DISTANCE - len);
  float power = fmaxf(-10.0f * (x + 0.52f) * (x * x) * (x - 0.43f), 0.0f);

  vec3_norm(r, direction);
  vec3_scale(r, r, power);

  // Blobs only fall with gravity, they don't force each other to fall
  if (r[1] < 0.0f) {
    r[1] = 0.0f;
  }
}