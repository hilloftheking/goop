#version 430

layout(location = 0) in vec3 local_pos;

layout(location = 0) out vec4 out_color;

layout(binding = 0) uniform samplerCube skybox;

void main() {
  out_color = texture(skybox, local_pos);
}