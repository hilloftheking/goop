#version 430

layout(location = 0) in vec3 in_pos;

layout(location = 0) out vec3 local_pos;

layout(location = 0) uniform mat4 view_mat;
layout(location = 1) uniform mat4 proj_mat;

void main() {
  local_pos = in_pos;

  mat4 view_center = view_mat;
  view_center[3].xyz = vec3(0.0);
  gl_Position = proj_mat * view_center * vec4(in_pos, 1.0);
}