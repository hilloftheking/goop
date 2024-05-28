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

#define DIST_MAX 100000000.0

uniform vec4 blobs[BLOB_COUNT];

float get_ray_sphere_intersection(vec3 ro, vec3 rd, vec3 center, float r) {
  float a = 1.0;
  float b = dot(2.0 * rd, ro - center);
  float c = dot(ro - center, ro - center) - r * r;
  float discriminant = b * b - 4 * a * c;

  if (discriminant > 0.0) {
    return (-b - sqrt(discriminant)) / (2.0 * a);
  }

  return DIST_MAX;
}

float get_blob_influence(vec3 ro, vec3 rd, vec3 center) {
  float ray_dist_from_center =
      sqrt(length(center - ro) * length(center - ro) -
           dot(center - ro, rd) * dot(center - ro, rd));
  return (BLOB_RADIUS + BLOB_SMOOTH - ray_dist_from_center) / BLOB_RADIUS;
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

  float tmin = DIST_MAX;

  vec3 normal = vec3(0.0);
  vec3 tnormal = vec3(0.0);

  for (int i = 0; i < BLOB_COUNT; i++) {
    vec3 center = blobs[i].xyz;
    float r = BLOB_RADIUS + BLOB_SMOOTH;

    float dist = get_ray_sphere_intersection(ro, rd, center, r);

    if (dist > 0.0 && dist < DIST_MAX) {
      if (dist >= tmin) {
        continue;
      }

      float influence = get_blob_influence(ro, rd, center);

      normal = normalize((ro + (dist + BLOB_SMOOTH) * rd) - center) * influence;

      if (influence >= 1.0) {
        if (dist < tmin) {
          tmin = dist;
          tnormal = normal;
        }
        continue;
      }

      vec3 point_in_sphere = ro + dot(center - ro, rd) * rd;

      for (int o = 0; o < BLOB_COUNT; o++) {
        if (o == i) {
          continue;
        }

        vec3 ocenter = blobs[o].xyz;
        if (length(ocenter - center) >= r * 2.0) {
          continue;
        }

        float odist = get_ray_sphere_intersection(ro, rd, ocenter, r);
        if (odist >= DIST_MAX) {
          continue;
        }

        vec3 opoint_in_sphere = ro + dot(ocenter - ro, rd) * rd;
        float point_dist = length(opoint_in_sphere - point_in_sphere);

        float oinfluence = max(0.0, get_blob_influence(ro, rd, ocenter) - point_dist);
        influence += oinfluence;

        normal += normalize((ro + odist * rd) - ocenter) * oinfluence;

        if (influence >= 1.0) {
          tmin = odist;
          tnormal = normal;
          break;
        }
      }
    }
  }

  tnormal = normalize(tnormal);

  if (tmin < DIST_MAX) {
    vec3 light_direction = -normalize(vec3(2.0, -5.0, 3.0));

    float diffuse_intensity =
        mix(max(0.0, dot(tnormal, light_direction)), 1.0, 0.2);

    out_color = vec4(vec3(BLOB_COLOR) * diffuse_intensity, 1.0);
  } else {
    out_color = vec4(0.0, 0.0, 0.0, 1.0);
  }

  //out_color = vec4(texture(picture, vec3(frag_pos, 0.5)).rgb, 1.0);
}