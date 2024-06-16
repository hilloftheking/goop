#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <glad/glad.h>

#include "blob.h"
#include "blob_render.h"
#include "shader_sources.h"

// Request dedicated GPU
__declspec(dllexport) unsigned long NvOptimusEnablement = 1;
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;

static GLuint compile_shader(const char *source, GLenum shader_type) {
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

static GLuint create_shader_program(const char *vert_shader_src,
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

static GLuint create_compute_program(const char *source) {
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

static vec3 cube_verts[] = {{0.5f, 0.5f, -0.5f},  {0.5f, -0.5f, -0.5f},
                            {0.5f, 0.5f, 0.5f},   {0.5f, -0.5f, 0.5f},
                            {-0.5f, 0.5f, -0.5f}, {-0.5f, -0.5f, -0.5f},
                            {-0.5f, 0.5f, 0.5f},  {-0.5f, -0.5f, 0.5f}};
static uint8_t cube_indices[] = {4, 2, 0, 2, 7, 3, 6, 5, 7, 1, 7, 5,
                                 0, 3, 1, 4, 1, 5, 4, 6, 2, 2, 6, 7,
                                 6, 4, 5, 1, 3, 7, 0, 2, 3, 4, 0, 1};

void blob_renderer_create(BlobRenderer *br) {
  glGenVertexArrays(1, &br->vao);
  glBindVertexArray(br->vao);

  glGenBuffers(1, &br->vbo);
  glBindBuffer(GL_ARRAY_BUFFER, br->vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(cube_verts), cube_verts, GL_STATIC_DRAW);

  glGenBuffers(1, &br->ibo);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, br->ibo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cube_indices), cube_indices,
               GL_STATIC_DRAW);

  br->raymarch_program =
      create_shader_program(RAYMARCH_VERT_SRC, RAYMARCH_FRAG_SRC);
  glUseProgram(br->raymarch_program);

  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, NULL);

  br->compute_program = create_compute_program(COMPUTE_SDF_COMP_SRC);

  glGenTextures(1, &br->sdf_tex);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_3D, br->sdf_tex);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA8, BLOB_SDF_RES, BLOB_SDF_RES,
               BLOB_SDF_RES, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glBindImageTexture(0, br->sdf_tex, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA8);

  glGenTextures(1, &br->sdf_char_tex);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_3D, br->sdf_char_tex);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA8, BLOB_SDF_RES, BLOB_SDF_RES,
               BLOB_SDF_RES, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glBindImageTexture(0, br->sdf_char_tex, 0, GL_TRUE, 0, GL_WRITE_ONLY,
                     GL_RGBA8);

  br->blobs_ssbo_size_bytes = sizeof(vec4) * BLOB_MAX_COUNT;
  glGenBuffers(1, &br->blobs_ssbo);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, br->blobs_ssbo);
  glBufferData(GL_SHADER_STORAGE_BUFFER, br->blobs_ssbo_size_bytes, NULL,
               GL_DYNAMIC_DRAW);

  br->blob_ot_ssbo_size_bytes = blob_ot_get_alloc_size();
  glGenBuffers(1, &br->blob_ot_ssbo);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, br->blob_ot_ssbo);
  glBufferData(GL_SHADER_STORAGE_BUFFER, br->blob_ot_ssbo_size_bytes, NULL,
               GL_DYNAMIC_DRAW);

  br->aspect_ratio = 1.0f;

  br->blob_ot = blob_ot_create();
  br->blobs_lerped = malloc(BLOB_MAX_COUNT * sizeof(*br->blobs_lerped));
}

void blob_renderer_destroy(BlobRenderer *br) {
  // TODO
}

static void draw_sdf_cube(BlobRenderer *br, unsigned int sdf_tex,
                          const vec3 pos, float size, float sdf_max_dist) {
  glBindTexture(GL_TEXTURE_3D, sdf_tex);

  mat4x4 model_mat;
  mat4x4_identity(model_mat);
  mat4x4_scale(model_mat, model_mat, size);
  vec3_dup(model_mat[3], pos);
  model_mat[3][3] = 1.0f;

  glUniformMatrix4fv(0, 1, GL_FALSE, model_mat[0]);
  glUniform1f(4, 1.0f / size);
  glUniform1f(5, sdf_max_dist);
  glDrawElements(GL_TRIANGLES, sizeof(cube_indices) / sizeof(*cube_indices),
                 GL_UNSIGNED_BYTE, NULL);
}

void blob_render(BlobRenderer *br, const BlobSimulation *bs) {
  blob_ot_reset(br->blob_ot);

  // Loop backwards so that new blobs are prioritized in the octree
  for (int i = bs->blob_count - 1; i >= 0; i--) {
    float *pos_lerp = br->blobs_lerped[i];
    for (int x = 0; x < 3; x++) {
      float diff = bs->blobs[i].pos[x] - bs->blobs[i].prev_pos[x];
      float diff_delta =
          diff * (float)((BLOB_TICK_TIME - bs->tick_timer) / BLOB_TICK_TIME);

      pos_lerp[x] = bs->blobs[i].prev_pos[x] + diff_delta;
    }

    pos_lerp[3] =
        (float)((int)(bs->blobs[i].radius * BLOB_RADIUS_MULT) * BLOB_MAT_COUNT +
                bs->blobs[i].mat_idx);

    blob_ot_insert(br->blob_ot, pos_lerp, i);
  }

  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, br->blobs_ssbo);
  glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, br->blobs_ssbo_size_bytes,
                  br->blobs_lerped);

  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, br->blob_ot_ssbo);
  glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, br->blob_ot_ssbo_size_bytes,
                  br->blob_ot);

  glBindImageTexture(0, br->sdf_tex, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA8);
  glUseProgram(br->compute_program);
  glUniform1i(0, -1); // Use the octree
  glUniform3fv(1, 1, BLOB_SIM_POS);
  glUniform1f(2, BLOB_SIM_SIZE);
  glUniform1f(3, BLOB_SDF_MAX_DIST);
  glUniform1f(4, BLOB_SMOOTH);
  glDispatchCompute(BLOB_SDF_RES / BLOB_SDF_LOCAL_GROUP_COUNT,
                    BLOB_SDF_RES / BLOB_SDF_LOCAL_GROUP_COUNT,
                    BLOB_SDF_RES / BLOB_SDF_LOCAL_GROUP_COUNT);

  vec4 model_blob_v4[MODEL_BLOB_MAX_COUNT];
  vec3 model_blob_min = {100, 100, 100};
  vec3 model_blob_max = {-100, -100, -100};
  for (int i = 0; i < bs->model_blob_count; i++) {
    ModelBlob *b = &bs->model_blobs[i];

    vec3_dup(model_blob_v4[i], b->pos);
    model_blob_v4[i][3] =
        (float)((int)(b->radius * BLOB_RADIUS_MULT) * BLOB_MAT_COUNT +
                b->mat_idx);

    for (int x = 0; x < 3; x++) {
      float c = b->pos[x];
      if (c - b->radius - BLOB_CHAR_SMOOTH < model_blob_min[x]) {
        model_blob_min[x] = c - b->radius - BLOB_CHAR_SMOOTH;
      }
      if (c + b->radius + BLOB_CHAR_SMOOTH > model_blob_max[x]) {
        model_blob_max[x] = c + b->radius + BLOB_CHAR_SMOOTH;
      }
    }
  }
  float model_blob_size = 0.0f;
  for (int i = 0; i < 3; i++) {
    float diff = model_blob_max[i] - model_blob_min[i];
    if (diff > model_blob_size) {
      model_blob_size = diff;
    }
  }
  vec3 model_blob_pos;
  vec3_add(model_blob_pos, model_blob_min, model_blob_max);
  vec3_scale(model_blob_pos, model_blob_pos, 0.5f);

  // TODO: use a different SSBO and image unit per dispatch?
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, br->blobs_ssbo);
  glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(model_blob_v4),
                  model_blob_v4);
  glBindImageTexture(0, br->sdf_char_tex, 0, GL_TRUE, 0, GL_WRITE_ONLY,
                     GL_RGBA8);
  glUniform1i(0, bs->model_blob_count);
  glUniform3fv(1, 1, model_blob_pos);
  glUniform1f(2, model_blob_size);
  glUniform1f(3, MODEL_BLOB_SDF_MAX_DIST);
  glUniform1f(4, BLOB_CHAR_SMOOTH);
  glDispatchCompute(BLOB_SDF_RES / BLOB_SDF_LOCAL_GROUP_COUNT,
                    BLOB_SDF_RES / BLOB_SDF_LOCAL_GROUP_COUNT,
                    BLOB_SDF_RES / BLOB_SDF_LOCAL_GROUP_COUNT);

  mat4x4 view_mat;
  mat4x4_invert(view_mat, br->cam_trans);

  mat4x4 proj_mat;
  mat4x4_perspective(proj_mat, 60.0f * (M_PI / 180.0f), br->aspect_ratio, 0.1f,
                     100.0f);

  glBindVertexArray(br->vao);

  glUseProgram(br->raymarch_program);
  glUniformMatrix4fv(1, 1, GL_FALSE, view_mat[0]);
  glUniformMatrix4fv(2, 1, GL_FALSE, proj_mat[0]);
  glUniform3fv(3, 1, br->cam_trans[3]);

  glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
  draw_sdf_cube(br, br->sdf_tex, BLOB_SIM_POS, BLOB_SIM_SIZE,
                BLOB_SDF_MAX_DIST);
  draw_sdf_cube(br, br->sdf_char_tex, model_blob_pos, model_blob_size,
                MODEL_BLOB_SDF_MAX_DIST);
}