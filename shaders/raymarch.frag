#version 430 core

#include "../src/blob_defines.h"

layout(location = 0) in vec2 frag_pos;

layout(location = 0) out vec4 out_color;

layout(location = 0, binding = 0) uniform sampler3D sdf_tex;
layout(location = 1) uniform mat4 cam_trans;
layout(location = 2) uniform float cam_fov;
layout(location = 3) uniform float cam_aspect;

#define MARCH_STEPS 128
#define MARCH_INTERSECT 0.001
#define MARCH_MAX_DIST 100.0
#define MARCH_NORM_STEP 0.2

#define BG_COLOR 0.05, 0.03, 0.1
#define BRIGHTNESS 0.2

// Figure out the normal with a gradient
vec3 get_normal_at(vec3 p) {
  const vec3 small_step = vec3(MARCH_NORM_STEP / 20.0, 0.0, 0.0);
  vec3 uvw = (p + vec3(10, 0, 10)) / vec3(20);

  float gradient_x = texture(sdf_tex, uvw + small_step.xyy).r -
                     texture(sdf_tex, uvw - small_step.xyy).r;
  float gradient_y = texture(sdf_tex, uvw + small_step.yxy).r -
                     texture(sdf_tex, uvw - small_step.yxy).r;
  float gradient_z = texture(sdf_tex, uvw + small_step.yyx).r -
                     texture(sdf_tex, uvw - small_step.yyx).r;

  return normalize(vec3(gradient_x, gradient_y, gradient_z));
}

// Returns color
vec3 ray_march(vec3 ro, vec3 rd) {
  float traveled = 0.0;
  for (int i = 0; i < MARCH_STEPS; i++) {
    vec3 p = ro + rd * traveled;

    if (p.y <= 0.0) {
      break;
    }

    float dist = texture(sdf_tex, (p + vec3(10, 0, 10)) / vec3(20)).r;

    if (dist <= MARCH_INTERSECT) {
      vec3 normal = get_normal_at(p);
      const vec3 light_direction = -normalize(vec3(2.0, -5.0, 3.0));

      float diffuse_intensity =
          mix(max(0.0, dot(normal, light_direction)), 1.0, BRIGHTNESS);

      return vec3(BLOB_COLOR) * diffuse_intensity;
    }

    traveled += dist;

    if (traveled >= MARCH_MAX_DIST) {
      break;
    }
  }

  return vec3(BG_COLOR);
}

void main() {
  vec3 ro = vec3(cam_trans[3]);

  mat4 rot_mat = cam_trans;
  rot_mat[3] = vec4(0, 0, 0, 1);

  mat4 dir_mat = mat4(1.0);
  vec2 ray_offsets = frag_pos;
  ray_offsets.x *= cam_aspect;
  dir_mat[3] = vec4(ray_offsets * tan(radians(cam_fov) / 2.0), 1.0, 0.0);

  vec3 rd = normalize(vec3((rot_mat * dir_mat)[3]));

  out_color = vec4(ray_march(ro, rd), 1.0);
}