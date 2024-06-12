#version 430 core

layout(location = 0) in vec3 in_pos;

layout(location = 0) out vec3 local_pos;

layout(location = 0) uniform mat4 model_mat;
layout(location = 1) uniform mat4 view_mat;
layout(location = 2) uniform mat4 proj_mat;

void main() {
  local_pos = in_pos;
  gl_Position = proj_mat * view_mat * model_mat * vec4(in_pos, 1.0);
}