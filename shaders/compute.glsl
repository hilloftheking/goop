#version 430 core

layout (local_size_x = 4, local_size_y = 4, local_size_z = 4) in;

layout (r32f, location = 0) writeonly uniform image3D img_output;

#define BLOB_COUNT 200
#define BLOB_RADIUS 0.5
#define BLOB_RADIUS 0.5

layout (location = 1) uniform vec4 blobs[BLOB_COUNT];

float smin(float a, float b, float k) {
  float h = clamp(0.5 + 0.5 * (b - a) / k, 0., 1.);
  return mix(b, a, h) - k * h * (1.0 - h);
}

float dist_sphere(vec3 c, float r, vec3 p) { return length(p - c) - r; }

void main() {
  vec3 p = vec3(gl_GlobalInvocationID) * (40.0 / 128.0) - vec3(20.0, 0.0, 20.0);

  float value = dist_sphere(blobs[0].xyz, BLOB_RADIUS, p);
  for (int i = 1; i < BLOB_COUNT; i++) {
    value =
        smin(value, dist_sphere(blobs[i].xyz, BLOB_RADIUS, p), BLOB_RADIUS);
  }

  imageStore(img_output, ivec3(gl_GlobalInvocationID.xyz),
             vec4(value, 0, 0, 0));
}