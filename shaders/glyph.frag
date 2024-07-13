#version 430

layout(location = 0) in vec2 uv;

layout(location = 0) out vec4 out_color;

layout(binding = 0) uniform sampler2D font_tex;

layout(location = 4) uniform vec4 color;

void main() {
  out_color = vec4(color.rgb, color.a * texture(font_tex, uv).r);
}
