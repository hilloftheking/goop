#version 330 core

in vec2 in_pos;

out vec2 frag_pos;

void main() {
  frag_pos = in_pos;
  gl_Position = vec4(in_pos, 0.0, 1.0);
}