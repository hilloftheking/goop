#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glad/glad.h>

#include "shader.h"

GLuint compile_shader(const char *source, GLenum shader_type) {
  int size = (int)strlen(source);

  GLuint shader = glCreateShader(shader_type);
  glShaderSource(shader, 1, &source, &size);
  glCompileShader(shader);

  int compile_status = 0;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &compile_status);

  // Compilation failed
  // TODO: fallback shader
  if (compile_status == GL_FALSE) {
    int log_length = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);

    char *message = malloc(log_length);
    glGetShaderInfoLog(shader, log_length, NULL, message);

    fprintf(stderr, "Shader compilation failed: %s\n", message);
    free(message);

    glDeleteShader(shader);
    return 0;
  }

  return shader;
}

GLuint create_shader_program(const char *vert_shader_src,
                             const char *frag_shader_src) {
  GLuint vert_shader = compile_shader(vert_shader_src, GL_VERTEX_SHADER);
  if (!vert_shader) {
    // TODO: Better error handling
    return 0;
  }
  GLuint frag_shader = compile_shader(frag_shader_src, GL_FRAGMENT_SHADER);
  if (!frag_shader) {
    return 0;
  }

  GLuint shader_program = glCreateProgram();
  glAttachShader(shader_program, vert_shader);
  glAttachShader(shader_program, frag_shader);
  // Shader objects only have to exist for this call
  glLinkProgram(shader_program);

  glDetachShader(shader_program, vert_shader);
  glDetachShader(shader_program, frag_shader);
  glDeleteShader(vert_shader);
  glDeleteShader(frag_shader);

  int link_status = 0;
  glGetProgramiv(shader_program, GL_LINK_STATUS, &link_status);

  // Linking failed
  if (link_status == GL_FALSE) {
    int log_length = 0;
    glGetProgramiv(shader_program, GL_INFO_LOG_LENGTH, &log_length);

    char *message = malloc(log_length);
    glGetProgramInfoLog(shader_program, log_length, NULL, message);

    fprintf(stderr, "Shader linking: %s\n", message);
    free(message);

    glDeleteProgram(shader_program);
    return 0;
  }

  return shader_program;
}

GLuint create_compute_program(const char *source) {
  GLuint shader = compile_shader(source, GL_COMPUTE_SHADER);
  if (!shader) {
    // TODO: Better error handling
    return 0;
  }

  GLuint shader_program = glCreateProgram();
  glAttachShader(shader_program, shader);
  // Shader objects only have to exist for this call
  glLinkProgram(shader_program);

  glDetachShader(shader_program, shader);
  glDeleteShader(shader);

  int link_status = 0;
  glGetProgramiv(shader_program, GL_LINK_STATUS, &link_status);

  // Linking failed
  if (link_status == GL_FALSE) {
    int log_length = 0;
    glGetProgramiv(shader_program, GL_INFO_LOG_LENGTH, &log_length);

    char *message = malloc(log_length);
    glGetProgramInfoLog(shader_program, log_length, NULL, message);

    fprintf(stderr, "Shader linking: %s\n", message);
    free(message);

    glDeleteProgram(shader_program);
    return 0;
  }

  return shader_program;
}