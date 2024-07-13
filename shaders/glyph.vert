#version 430

layout(location = 0) in vec2 in_pos;

layout(location = 0) out vec2 uv;

layout(location = 0) uniform vec2 pos;
layout(location = 1) uniform vec2 scale;
layout(location = 2) uniform vec2 uv_start;
layout(location = 3) uniform vec2 uv_size;

void main() {
  uv = uv_start + uv_size * (in_pos + vec2(0.5));
  gl_Position = vec4(in_pos * scale + pos, 0, 1);
}
