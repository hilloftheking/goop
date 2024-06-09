#version 430 core

#include "../src/blob_defines.h"

layout(location = 0) in vec2 frag_pos;

layout(location = 0) out vec4 out_color;

layout(binding = 0) uniform sampler3D sdf_tex;

layout(location = 0) uniform mat4 cam_trans;
layout(location = 1) uniform float cam_fov;
layout(location = 2) uniform float cam_aspect;

#define MARCH_STEPS 128
#define MARCH_INTERSECT 0.001
#define MARCH_MAX_DIST 40.0
#define MARCH_NORM_STEP 0.2

#define BG_COLOR 0.3, 0.3, 0.3
#define BRIGHTNESS 0.2

vec3 colors[] = {
  vec3(LIQUID_COLOR),
  vec3(SOLID_COLOR),
  vec3(CHAR_COLOR)
};

// Figure out the normal with a gradient
vec3 get_normal_at(vec3 p, int dist_idx) {
  const vec3 small_step = vec3(MARCH_NORM_STEP / vec3(BLOB_SDF_SIZE).x, 0.0, 0.0);
  vec3 uvw = (p - vec3(BLOB_SDF_START)) / vec3(BLOB_SDF_SIZE);

  int i = dist_idx;

  float gradient_x = texture(sdf_tex, uvw + small_step.xyy)[i] -
                     texture(sdf_tex, uvw - small_step.xyy)[i];
  float gradient_y = texture(sdf_tex, uvw + small_step.yxy)[i] -
                     texture(sdf_tex, uvw - small_step.yxy)[i];
  float gradient_z = texture(sdf_tex, uvw + small_step.yyx)[i] -
                     texture(sdf_tex, uvw - small_step.yyx)[i];

  return normalize(vec3(gradient_x, gradient_y, gradient_z));
}

vec2 intersect_aabb(vec3 ro, vec3 rd, vec3 bmin, vec3 bmax) {
    vec3 tmin = (bmin - ro) / rd;
    vec3 tmax = (bmax - ro) / rd;
    vec3 t1 = min(tmin, tmax);
    vec3 t2 = max(tmin, tmax);
    float tnear = max(max(t1.x, t1.y), t1.z);
    float tfar = min(min(t2.x, t2.y), t2.z);
    return vec2(tnear, tfar);
}

// Returns color
vec3 ray_march(vec3 ro, vec3 rd) {
  vec2 near_far = intersect_aabb(ro, rd, vec3(BLOB_SDF_START), vec3(BLOB_SDF_SIZE) + vec3(BLOB_SDF_START));
  if (near_far[0] > near_far[1] || near_far[1] < 0.0) {
    return vec3(0.0, 0.0, 0.0);
    //return vec3(BG_COLOR);
  }

  float traveled = max(0.0, near_far[0]);

  for (int i = 0; i < MARCH_STEPS; i++) {
    vec3 p = ro + rd * traveled;

    vec3 dists = texture(sdf_tex, (p - vec3(BLOB_SDF_START)) / vec3(BLOB_SDF_SIZE)).rgb;
    float dist = dists[0];
    int cmin = 0;
    for (int i = 1; i < 3; i++) {
      if (dists[i] < dist) {
        cmin = i;
        dist = dists[i];
      }
    }

    if (dist <= MARCH_INTERSECT) {
      // TODO: Getting the normal is kind of expensive
      // Maybe it could be possible to have a low quality normal in the SDF
      vec3 normal = get_normal_at(p, cmin);
      const vec3 light_direction = -normalize(vec3(2.0, -5.0, 3.0));

      float diffuse_intensity =
          mix(max(0.0, dot(normal, light_direction)), 1.0, BRIGHTNESS);
      
      return colors[cmin] * diffuse_intensity;
    }

    traveled += dist;

    if (traveled >= MARCH_MAX_DIST) {
      break;
    }

    if (traveled >= near_far[1]) {
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