#version 430 core

#include "../src/blob_defines.h"

layout(location = 0) in vec3 world_pos;

layout(location = 0) out vec4 out_color;

layout(binding = 0) uniform sampler3D sdf_tex;

layout(location = 3) uniform vec3 cam_pos;

#define MARCH_STEPS 128
#define MARCH_INTERSECT 0.001
#define MARCH_MAX_DIST 40.0
#define MARCH_NORM_STEP 0.01

#define BG_COLOR 0.3, 0.3, 0.3
#define BRIGHTNESS 0.2

// Figure out the normal with a gradient
vec3 get_normal_at(vec3 p) {
  const vec3 small_step = vec3(MARCH_NORM_STEP, 0.0, 0.0);
  vec3 uvw = (p - vec3(BLOB_SDF_START)) / vec3(BLOB_SDF_SIZE);

  float gradient_x = texture(sdf_tex, uvw + small_step.xyy).a -
                     texture(sdf_tex, uvw - small_step.xyy).a;
  float gradient_y = texture(sdf_tex, uvw + small_step.yxy).a -
                     texture(sdf_tex, uvw - small_step.yxy).a;
  float gradient_z = texture(sdf_tex, uvw + small_step.yyx).a -
                     texture(sdf_tex, uvw - small_step.yyx).a;

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
  if (near_far[1] < 0.00) {
    return vec3(0.0, 0.0, 0.0);
  }

  float traveled = max(0.0, near_far[0]);

  for (int i = 0; i < MARCH_STEPS; i++) {
    vec3 p = ro + rd * traveled;

    vec4 dat = texture(sdf_tex, (p - vec3(BLOB_SDF_START)) / vec3(BLOB_SDF_SIZE));
    float dist = (dat.a * (BLOB_SDF_MAX_DIST + 0.5)) - 0.5;

    if (dist <= MARCH_INTERSECT) {
      // TODO: Getting the normal is kind of expensive
      // Maybe it could be possible to have a low quality normal in the SDF
      vec3 normal = get_normal_at(p);

      const vec3 light0_dir = -normalize(vec3(2.0, -5.0, 3.0));

      const vec3 light1_dir = -normalize(vec3(-1.0, 5.0, -1.0));
      const vec3 light1_col = vec3(0.05, 0.05, 0.08);

      float light0_val =
          mix(max(0.0, dot(normal, light0_dir)), 1.0, BRIGHTNESS);
      float light1_val =
          mix(max(0.0, dot(normal, light1_dir)), 1.0, BRIGHTNESS);
      
      return dat.rgb * light0_val + light1_col * light1_val;
    }

    traveled += dist;

    if (traveled >= MARCH_MAX_DIST || traveled >= near_far[1]) {
      break;
    }
  }

  return vec3(BG_COLOR);
}

void main() {
  vec3 ro = cam_pos;
  vec3 rd = normalize(world_pos - ro);

  out_color = vec4(ray_march(ro, rd), 1.0);
  //out_color = texture(sdf_tex, vec3(0.5));
}