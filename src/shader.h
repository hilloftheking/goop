#pragma once

unsigned int compile_shader(const char *source, unsigned int shader_type);

unsigned int create_shader_program(const char *vert_shader_src,
                                   const char *frag_shader_src);

unsigned int create_compute_program(const char *source);