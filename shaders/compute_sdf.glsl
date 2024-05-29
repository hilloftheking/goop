#version 430 core

layout (local_size_x = 4, local_size_y = 4, local_size_z = 4) in;

layout (r32f, location = 0, binding = 0) writeonly uniform image3D img_output;
layout (rgba32f, location = 1, binding = 1) readonly uniform image1D blobs_tex;

layout (location = 2) uniform int blob_count;

#define BLOB_RADIUS 0.5
#define BLOB_SMOOTH 0.5

#define BLOB_SDF_RES 96

float smin(float a, float b, float k) {
  float h = clamp(0.5 + 0.5 * (b - a) / k, 0., 1.);
  return mix(b, a, h) - k * h * (1.0 - h);
}

float dist_sphere(vec3 c, float r, vec3 p) { return length(p - c) - r; }

void main() {
  vec3 p = vec3(gl_GlobalInvocationID) * (20.0 / BLOB_SDF_RES) -
           vec3(10.0, 0.0, 10.0);

  float value = dist_sphere(imageLoad(blobs_tex, 0).xyz, BLOB_RADIUS, p);
  for (int i = 1; i < blob_count; i++) {
    value =
        smin(value, dist_sphere(imageLoad(blobs_tex, i).xyz, BLOB_RADIUS, p),
             BLOB_SMOOTH);
  }

  imageStore(img_output, ivec3(gl_GlobalInvocationID.xyz),
             vec4(value, 0, 0, 0));
}