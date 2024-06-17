#version 430

#include "../src/blob_defines.h"

layout(location = 0) in vec3 local_pos;

layout(location = 0) out vec4 out_color;

layout(binding = 0) uniform sampler3D sdf_tex;

layout(location = 0) uniform mat4 model_mat;
layout(location = 1) uniform mat4 view_mat;
layout(location = 2) uniform mat4 proj_mat;
layout(location = 3) uniform vec3 cam_pos;
layout(location = 4) uniform float dist_scale;
layout(location = 5) uniform float sdf_max_dist;

#define MARCH_STEPS 128
#define MARCH_INTERSECT 0.001
#define MARCH_MAX_DIST 40.0
#define MARCH_NORM_STEP 0.2

#define BRIGHTNESS 0.4

// Figure out the normal with a gradient
vec3 get_normal_at(vec3 p) {
  const vec3 small_step = vec3(MARCH_NORM_STEP * dist_scale, 0.0, 0.0);
  vec3 uvw = p + vec3(0.5);

  float gradient_x = texture(sdf_tex, uvw + small_step.xyy).a -
                     texture(sdf_tex, uvw - small_step.xyy).a;
  float gradient_y = texture(sdf_tex, uvw + small_step.yxy).a -
                     texture(sdf_tex, uvw - small_step.yxy).a;
  float gradient_z = texture(sdf_tex, uvw + small_step.yyx).a -
                     texture(sdf_tex, uvw - small_step.yyx).a;

  return -normalize(vec3(gradient_x, gradient_y, gradient_z));
}

// Returns depth at local position within model
float get_depth_at(vec3 p) {
  // Just going to assume this will always be the case
  const float dnear = 0.0;
  const float dfar = 1.0;

  vec4 clip_pos = proj_mat * view_mat * model_mat * vec4(p, 1.0);
  float ndc_depth = clip_pos.z / clip_pos.w;
  return (((dfar - dnear) * ndc_depth) + dnear + dfar) / 2.0;
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

// Returns color and distance. Negative distance means it should be discarded
vec4 ray_march(vec3 ro, vec3 rd) {
  vec2 near_far = intersect_aabb(ro, rd, vec3(-0.5), vec3(0.5));

  float traveled = max(0.0, near_far[0]);

  for (int i = 0; i < MARCH_STEPS; i++) {
    vec3 p = ro + rd * traveled;

    vec4 dat = texture(sdf_tex, p + vec3(0.5));
    float dist = (((1.0 - dat.a) * (sdf_max_dist - BLOB_SDF_MIN_DIST)) + BLOB_SDF_MIN_DIST) * dist_scale;

    if (dist <= MARCH_INTERSECT) {
      // TODO: Getting the normal is kind of expensive
      // Maybe it could be possible to have a low quality normal in the SDF
      vec3 normal = get_normal_at(p);

      const vec3 light0_dir = -normalize(vec3(2.0, -5.0, -3.0));
      const vec3 light0_col = vec3(1.0, 1.0, 0.9);

      const vec3 light1_dir = -normalize(vec3(-1.0, 5.0, 2.0));
      const vec3 light1_col = vec3(0.05, 0.05, 0.08);

      float light0_val =
          mix(max(0.0, dot(normal, light0_dir)), 1.0, BRIGHTNESS);
      float light1_val =
          mix(max(0.0, dot(normal, light1_dir)), 1.0, BRIGHTNESS);
      
      vec3 color = dat.rgb * light0_col * light0_val + dat.rgb * light1_col * light1_val;

      return vec4(color, traveled);
    }

    traveled += dist;

    if (traveled >= MARCH_MAX_DIST || traveled >= near_far[1]) {
      break;
    }
  }

  // Discarding outside of main() seems to not work on intel
  return vec4(0, 0, 0, -1);
}

void main() {
  vec3 cam_mdl_pos = (inverse(model_mat) * vec4(cam_pos, 1.0)).xyz;

  vec3 ro = cam_mdl_pos;
  vec3 rd = normalize(local_pos - ro);

  vec4 result = ray_march(ro, rd);
  if (result.a < 0.0)
    discard;

  out_color = vec4(result.rgb, 1.0);
  gl_FragDepth = get_depth_at(ro + rd * result.a);
}