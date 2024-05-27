#version 430 core

layout (local_size_x = 32, local_size_y = 32, local_size_z = 1) in;

layout (rgba8ui, binding = 0) readonly uniform uimage2D img_input;
layout (rgba8ui, binding = 1) writeonly uniform uimage2D img_output;

void main() {
  ivec4 value = ivec4(0, 0, 0, 255);

  ivec2 tex_coord = ivec2(gl_GlobalInvocationID.xy);
  value.rgb = ivec3(255, 255, 255) - ivec3(imageLoad(img_input, tex_coord).rgb);
  //value.rgb = vec3(1, 0, 0);

  imageStore(img_output, tex_coord, value);
}