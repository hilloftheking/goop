#version 330 core

in vec2 frag_pos;

out vec4 out_color;

uniform mat4 cam_trans;
uniform float cam_fov;
uniform float cam_aspect;

#define BLOB_COUNT 200
#define BLOB_RADIUS 0.5
#define BLOB_SMOOTH 0.5
#define BLOB_COLOR 1.0, 1.0, 0.8

#define MARCH_STEPS 128
#define MARCH_INTERSECT 0.01
#define MARCH_MAX_DIST 100.0
#define MARCH_NORM_STEP 0.2

#define BG_COLOR 0.05, 0.03, 0.1
#define BRIGHTNESS 0.2

uniform vec4 blobs[BLOB_COUNT];

uniform sampler3D sdf_tex;

// Figure out the normal with a gradient
vec3 get_normal_at(vec3 p) {
  const vec3 small_step = vec3(MARCH_NORM_STEP / 40.0, 0.0, 0.0);
  vec3 uvw = (p + vec3(20, 0, 20)) / vec3(40);

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

    if (abs(p.x) >= 20.0 || p.y <= 0.0 || p.y >= 40.0 || abs(p.z) >= 20.0) {
      break;
    }

    float dist = texture(sdf_tex, (p + vec3(20, 0, 20)) / vec3(40)).r;

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