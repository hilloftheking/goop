#version 330 core

in vec2 frag_pos;

out vec4 out_color;

uniform mat4 cam_trans;
uniform float cam_fov;
uniform float cam_aspect;

uniform sampler3D picture;

#define BLOB_COUNT 20
#define BLOB_RADIUS 0.5
#define BLOB_SMOOTH 0.7
#define BLOB_COLOR 1.0, 1.0, 0.8

#define SOLID_COUNT 200
#define SOLID_RADIUS 0.5
#define SOLID_SMOOTH 0.5
#define SOLID_COLOR 0.5, 1.0, 0.3

#define BRIGHTNESS 0.3

uniform vec4 blobs[BLOB_COUNT];
uniform vec4 solids[SOLID_COUNT];

float distance_from_sphere(vec3 p, vec3 c, float r) {
  return length(p - c) - r;
}

float smin(float a, float b, float k) {
  float h = clamp(0.5 + 0.5 * (b - a) / k, 0., 1.);
  return mix(b, a, h) - k * h * (1.0 - h);
}

vec4 map_the_world(vec3 p) {
  float value = distance_from_sphere(p, blobs[0].xyz, BLOB_RADIUS);
  for (int i = 1; i < BLOB_COUNT; i++) {
    value = smin(value, distance_from_sphere(p, blobs[i].xyz, BLOB_RADIUS), BLOB_SMOOTH);
  }

  float solid_value = texture(picture, p / 10.0).r * 10.0;
  if (solid_value < 0.01) {
    solid_value = distance_from_sphere(p, solids[0].xyz, SOLID_RADIUS);
    for (int i = 1; i < SOLID_COUNT; i++) {
      solid_value = smin(solid_value,
                         distance_from_sphere(p, solids[i].xyz, SOLID_RADIUS),
                         SOLID_SMOOTH);
    }
  }

  vec3 color;
  if (value < solid_value) {
    color = vec3(BLOB_COLOR);
  } else {
    color = vec3(SOLID_COLOR);
  }

  return vec4(min(value, solid_value), color);
}

vec3 calculate_normal(vec3 p) {
  const vec3 small_step = vec3(0.001, 0.0, 0.0);

  float gradient_x =
      map_the_world(p + small_step.xyy).x - map_the_world(p - small_step.xyy).x;
  float gradient_y =
      map_the_world(p + small_step.yxy).x - map_the_world(p - small_step.yxy).x;
  float gradient_z =
      map_the_world(p + small_step.yyx).x - map_the_world(p - small_step.yyx).x;

  vec3 normal = vec3(gradient_x, gradient_y, gradient_z);

  return normalize(normal);
}

vec3 ray_march(vec3 ro, vec3 rd) {
  float total_distance_traveled = 0.0;
  const int NUMBER_OF_STEPS = 128;
  const float MINIMUM_HIT_DISTANCE = 0.001;
  const float MAXIMUM_TRACE_DISTANCE = 1000.0;

  for (int i = 0; i < NUMBER_OF_STEPS; ++i) {
    vec3 current_position = ro + total_distance_traveled * rd;

    vec4 map_info = map_the_world(current_position);
    float distance_to_closest = map_info[0];
    vec3 color = map_info.gba;

    if (distance_to_closest < MINIMUM_HIT_DISTANCE) {
      vec3 norm = calculate_normal(current_position);
      
      vec3 light_direction = -normalize(vec3(2.0, -5.0, 3.0));

      float diffuse_intensity =
          mix(max(0.0, dot(norm, light_direction)), 1.0, BRIGHTNESS);

      return color * diffuse_intensity;
    }

    if (total_distance_traveled > MAXIMUM_TRACE_DISTANCE) {
      break;
    }

    total_distance_traveled += distance_to_closest;
  }

  return vec3(0.0);
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
  //out_color = vec4(texture(picture, vec3(frag_pos, 0.5)).rgb, 1.0);
}