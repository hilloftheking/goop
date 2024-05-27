#version 430 core

layout (local_size_x = 4, local_size_y = 4, local_size_z = 4) in;

layout (r8ui, location = 0) writeonly uniform uimage3D img_output;

#define SOLID_COUNT 200
#define SOLID_RADIUS 0.5
#define SOLID_SMOOTH 0.5

layout (location = 1) uniform vec4 solids[SOLID_COUNT];

float distance_from_sphere(vec3 p, vec3 c, float r) {
  return length(p - c) - r;
}

float smin(float a, float b, float k) {
  float h = clamp(0.5 + 0.5 * (b - a) / k, 0., 1.);
  return mix(b, a, h) - k * h * (1.0 - h);
}

void main() {
  vec3 p = vec3(gl_GlobalInvocationID) * (10.0 / 128.0);

  float value = distance_from_sphere(p, solids[0].xyz, SOLID_RADIUS);
  for (int i = 1; i < SOLID_COUNT; i++) {
    value = smin(value, distance_from_sphere(p, solids[i].xyz, SOLID_RADIUS),
                 SOLID_SMOOTH);
  }

  int r = min(int(floor((max(0.0, value) / 10.0) * 255.0)), 255);

  imageStore(img_output, ivec3(gl_GlobalInvocationID.xyz),
             ivec4(r, 0, 0, 255));
}