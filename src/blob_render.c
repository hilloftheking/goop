#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include <glad/glad.h>

#include "blob.h"
#include "blob_render.h"
#include "shader_sources.h"

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

void blob_renderer_create(BlobRenderer* br) {
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
  glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA32F, BLOB_SDF_RES, BLOB_SDF_RES,
               BLOB_SDF_RES, 0, GL_RGBA, GL_FLOAT, NULL);
  glBindImageTexture(0, br->sdf_tex, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA32F);

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

  br->compute_whole_sdf_this_frame = true;
}

void blob_renderer_destroy(BlobRenderer* br) {
  // TODO
}

void blob_render(BlobRenderer* br, const BlobSimulation* bs) {
  // Keep track of the minimum and maximum position of blobs so less of the
  // SDF has to be updated
  vec3 area_min = {1000, 1000, 1000};
  vec3 area_max = {-1000, -1000, -1000};

  float farthest_traveled = 0.0f;

  blob_ot_reset(br->blob_ot);

  // Loop backwards so that new blobs are prioritized in the octree
  for (int i = bs->blob_count - 1; i >= 0; i--) {
    float *pos_lerp = br->blobs_lerped[i];
    vec3 traveled;
    for (int x = 0; x < 3; x++) {
      float diff = bs->blobs[i].pos[x] - bs->blobs[i].prev_pos[x];
      float diff_delta =
          diff * (float)((BLOB_TICK_TIME - bs->tick_timer) / BLOB_TICK_TIME);

      pos_lerp[x] = bs->blobs[i].prev_pos[x] + diff_delta;
      traveled[x] = diff_delta;

      if (pos_lerp[x] < area_min[x]) {
        area_min[x] = pos_lerp[x];
      }
      if (pos_lerp[x] > area_max[x]) {
        area_max[x] = pos_lerp[x];
      }
    }

    pos_lerp[3] = (float)(bs->blobs[i].type);

    blob_ot_insert(br->blob_ot, pos_lerp, i);

    float dist = vec3_len(traveled);
    if (dist > farthest_traveled) {
      farthest_traveled = dist;
    }
  }

  /*
  BlobOtNode *root = blob_ot + 0;
  for (int i = 0; i < 8; i++) {
    for (int j = 0; j < 8; j++) {
      printf("Oct %d.%d blobs: %d\n", i, j,
             blob_ot[blob_ot[root->indices[i]].indices[j]].leaf_blob_count);
    }
  }
  */

  int num_groups[3] = {0};

  for (int i = 0; i < 3; i++) {
    area_min[i] =
        fmaxf(0.0f, floorf(area_min[i] - blob_sdf_start[i] - BLOB_SDF_MAX_DIST -
                           BLOB_RADIUS - BLOB_SMOOTH - farthest_traveled));
    area_max[i] = ceilf(area_max[i] - blob_sdf_start[i] + BLOB_SDF_MAX_DIST +
                        BLOB_RADIUS + BLOB_SMOOTH + farthest_traveled + 1.0f);

    num_groups[i] = (int)ceilf(
        ((area_max[i] - area_min[i]) * (BLOB_SDF_RES / blob_sdf_size[i])) /
        (float)BLOB_SDF_LOCAL_GROUPS);

    int max_groups = BLOB_SDF_RES / BLOB_SDF_LOCAL_GROUPS;
    if (num_groups[i] < 0) {
      num_groups[i] = 0;
    } else if (num_groups[i] > max_groups - area_min[i]) {
      num_groups[i] = (int)(max_groups - area_min[i]);
    }
  }

  /*
  printf("(%d, %d, %d) : (%d, %d, %d)\n", invocation_offset[0],
         invocation_offset[1], invocation_offset[2], num_groups[0],
         num_groups[1], num_groups[2]);
         */

  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, br->blobs_ssbo);
  glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, br->blobs_ssbo_size_bytes,
                  br->blobs_lerped);

  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, br->blob_ot_ssbo);
  glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, br->blob_ot_ssbo_size_bytes,
                  br->blob_ot);

  glUseProgram(br->compute_program);
  // TODO: Additional logic for blob characters is required to do partial SDF
  br->compute_whole_sdf_this_frame = true;
  if (br->compute_whole_sdf_this_frame) {
    //puts("Recalculating SDF...");
    br->compute_whole_sdf_this_frame = false;

    glUniform3i(0, 0, 0, 0);
    glDispatchCompute(BLOB_SDF_RES / BLOB_SDF_LOCAL_GROUPS,
                      BLOB_SDF_RES / BLOB_SDF_LOCAL_GROUPS,
                      BLOB_SDF_RES / BLOB_SDF_LOCAL_GROUPS);
  } else {
    glUniform3i(0, (int)area_min[0], (int)area_min[1], (int)area_min[2]);
    glDispatchCompute(num_groups[0], num_groups[1], num_groups[2]);
  }

  glUseProgram(br->raymarch_program);

  mat4x4 model_mat;
  mat4x4_identity(model_mat);
  mat4x4_scale(model_mat, model_mat, blob_sdf_size[0]);
  for (int i = 0; i < 3; i++) {
    model_mat[3][i] = blob_sdf_size[i] * 0.5f + blob_sdf_start[i];
  }
  model_mat[3][3] = 1.0f;

  mat4x4 view_mat;
  mat4x4_invert(view_mat, br->cam_trans);

  mat4x4 proj_mat;
  mat4x4_perspective(proj_mat, 60.0f * (3.14159f / 180.0f), br->aspect_ratio,
                     0.1f, 100.0f);

  glUniformMatrix4fv(0, 1, GL_FALSE, model_mat[0]);
  glUniformMatrix4fv(1, 1, GL_FALSE, view_mat[0]);
  glUniformMatrix4fv(2, 1, GL_FALSE, proj_mat[0]);
  glUniform3fv(3, 1, br->cam_trans[3]);

  glBindVertexArray(br->vao);
  glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
  glDrawElements(GL_TRIANGLES, sizeof(cube_indices) / sizeof(*cube_indices),
                 GL_UNSIGNED_BYTE, NULL);
}