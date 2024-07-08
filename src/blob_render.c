#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <glad/glad.h>

#include "stb/stb_image.h"

#include "blob.h"
#include "blob_render.h"
#include "cube.h"
#include "resource.h"
#include "resource_load.h"
#include "shader.h"
#include "shader_sources.h"

// Request dedicated GPU
__declspec(dllexport) unsigned long NvOptimusEnablement = 1;
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;

void blob_renderer_create(BlobRenderer *br) {
  br->raymarch_program =
      create_shader_program(RAYMARCH_VERT_SRC, RAYMARCH_FRAG_SRC);
  glUseProgram(br->raymarch_program);

  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, NULL);

  br->compute_program = create_compute_program(COMPUTE_SDF_COMP_SRC);

  glGenTextures(1, &br->sdf_tex);
  glBindTexture(GL_TEXTURE_3D, br->sdf_tex);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA8, BLOB_SIM_SDF_RES, BLOB_SIM_SDF_RES,
               BLOB_SIM_SDF_RES, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glBindImageTexture(0, br->sdf_tex, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA8);

  glGenTextures(1, &br->sdf_mdl_tex);
  glBindTexture(GL_TEXTURE_3D, br->sdf_mdl_tex);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA8, BLOB_MODEL_SDF_RES,
               BLOB_MODEL_SDF_RES, BLOB_MODEL_SDF_RES, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, NULL);
  glBindImageTexture(0, br->sdf_mdl_tex, 0, GL_TRUE, 0, GL_WRITE_ONLY,
                     GL_RGBA8);

  br->solids_ssbo_size_bytes = sizeof(HMM_Vec4) * BLOB_SIM_MAX_SOLIDS;
  glGenBuffers(1, &br->solids_ssbo);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, br->solids_ssbo);
  glBufferData(GL_SHADER_STORAGE_BUFFER, br->solids_ssbo_size_bytes, NULL,
               GL_DYNAMIC_DRAW);

  br->liquids_ssbo_size_bytes = sizeof(HMM_Vec4) * BLOB_SIM_MAX_LIQUIDS;
  glGenBuffers(1, &br->liquids_ssbo);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, br->liquids_ssbo);
  glBufferData(GL_SHADER_STORAGE_BUFFER, br->liquids_ssbo_size_bytes, NULL,
               GL_DYNAMIC_DRAW);

  br->solid_ot_ssbo_size_bytes = 2400000 * sizeof(int);
  glGenBuffers(1, &br->solid_ot_ssbo);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, br->solid_ot_ssbo);
  glBufferData(GL_SHADER_STORAGE_BUFFER, br->solid_ot_ssbo_size_bytes, NULL,
               GL_DYNAMIC_DRAW);

  br->liquid_ot_ssbo_size_bytes = 2400000 * sizeof(int);
  glGenBuffers(1, &br->liquid_ot_ssbo);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, br->liquid_ot_ssbo);
  glBufferData(GL_SHADER_STORAGE_BUFFER, br->liquid_ot_ssbo_size_bytes, NULL,
               GL_DYNAMIC_DRAW);

  br->aspect_ratio = 1.0f;

  br->solids_v4 = malloc(BLOB_SIM_MAX_SOLIDS * sizeof(*br->solids_v4));
  br->liquids_v4 = malloc(BLOB_SIM_MAX_LIQUIDS * sizeof(*br->liquids_v4));

  {
    Resource img;
    resource_load(&img, IDB_WATER, "JPG");

    int width, height;
    stbi_uc *pixels = stbi_load_from_memory(img.data, img.data_size, &width,
                                            &height, NULL, 4);

    glGenTextures(1, &br->water_tex);
    glBindTexture(GL_TEXTURE_2D, br->water_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, pixels);

    stbi_image_free(pixels);

    resource_destroy(&img);
  }

  {
    Resource img;
    resource_load(&img, IDB_WATER_NORMAL, "JPG");

    int width, height;
    stbi_uc *pixels = stbi_load_from_memory(img.data, img.data_size, &width,
                                            &height, NULL, 4);

    glGenTextures(1, &br->water_norm_tex);
    glBindTexture(GL_TEXTURE_2D, br->water_norm_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, pixels);

    stbi_image_free(pixels);

    resource_destroy(&img);
  }
}

void blob_renderer_destroy(BlobRenderer *br) {
  // TODO
}

// transform can be null
static void draw_sdf_cube(BlobRenderer *br, unsigned int sdf_tex,
                          const HMM_Vec3 *pos, float size,
                          const HMM_Mat4 *transform, float sdf_max_dist) {
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_3D, sdf_tex);
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, br->water_tex);
  glActiveTexture(GL_TEXTURE2);
  glBindTexture(GL_TEXTURE_2D, br->water_norm_tex);

  HMM_Mat4 model_mat = HMM_M4D(size);
  model_mat.Columns[3].XYZ = *pos;
  model_mat.Elements[3][3] = 1.0f;
  if (transform) {
    model_mat = HMM_MulM4(*transform, model_mat);
  }

  glUniformMatrix4fv(0, 1, GL_FALSE, model_mat.Elements[0]);
  glUniform1f(4, 1.0f / size);
  glUniform1f(5, sdf_max_dist);
  cube_draw();
}

void blob_render_sim(BlobRenderer *br, const BlobSim *bs) {
  // Solids
  for (int i = 0; i < bs->solids.count; i++) {
    const SolidBlob *b = fixed_array_get_const(&bs->solids, i);

    br->solids_v4[i].XYZ = b->pos;
    br->solids_v4[i].W =
        (float)((int)(b->radius * BLOB_RADIUS_MULT) * BLOB_MAT_COUNT +
                b->mat_idx);
  }

  // Liquids
  for (int i = 0; i < bs->liquids.count; i++) {
    const LiquidBlob *b = fixed_array_get_const(&bs->liquids, i);

    br->liquids_v4[i].XYZ = b->pos;
    br->liquids_v4[i].W =
        (float)((int)(b->radius * BLOB_RADIUS_MULT) * BLOB_MAT_COUNT +
                b->mat_idx);
  }

  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, br->solids_ssbo);
  glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, br->solids_ssbo_size_bytes,
                  br->solids_v4);

  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, br->liquids_ssbo);
  glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, br->liquids_ssbo_size_bytes,
                  br->liquids_v4);

  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, br->solid_ot_ssbo);
  glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0,
                  bs->solid_ot.size_int * sizeof(int), bs->solid_ot.root);

  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, br->liquid_ot_ssbo);
  glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0,
                  bs->liquid_ot.size_int * sizeof(int), bs->liquid_ot.root);

  glBindImageTexture(0, br->sdf_tex, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA8);
  glUseProgram(br->compute_program);
  glUniform1i(0, -1); // Use the octree
  glUniform3fv(1, 1, bs->active_pos.Elements);
  glUniform1f(2, BLOB_ACTIVE_SIZE);
  glUniform1i(3, BLOB_SIM_SDF_RES);
  glUniform1f(4, BLOB_SDF_MAX_DIST);
  glUniform1f(5, BLOB_SMOOTH);
  glUniform1f(6, bs->solid_ot.root_size);
  glDispatchCompute(BLOB_SIM_SDF_RES / BLOB_SDF_LOCAL_GROUP_COUNT,
                    BLOB_SIM_SDF_RES / BLOB_SDF_LOCAL_GROUP_COUNT,
                    BLOB_SIM_SDF_RES / BLOB_SDF_LOCAL_GROUP_COUNT);

  glBindVertexArray(cube_vao);

  glUseProgram(br->raymarch_program);
  glUniformMatrix4fv(1, 1, GL_FALSE, br->view_mat.Elements[0]);
  glUniformMatrix4fv(2, 1, GL_FALSE, br->proj_mat.Elements[0]);
  glUniform3fv(3, 1, br->cam_trans.Elements[3]);

  glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
  draw_sdf_cube(br, br->sdf_tex, &bs->active_pos, BLOB_ACTIVE_SIZE, NULL,
                BLOB_SDF_MAX_DIST);
}

void blob_render_mdl(BlobRenderer *br, const BlobSim *bs, const Model *mdl,
                     const HMM_Mat4 *trans) {
  HMM_Vec4 model_blob_v4[128];
  HMM_Vec3 model_blob_min = {100, 100, 100};
  HMM_Vec3 model_blob_max = {-100, -100, -100};
  for (int i = 0; i < mdl->blob_count; i++) {
    const ModelBlob *b = &mdl->blobs[i];

    model_blob_v4[i].XYZ = b->pos;
    model_blob_v4[i].W =
        (float)((int)(b->radius * BLOB_RADIUS_MULT) * BLOB_MAT_COUNT +
                b->mat_idx);

    for (int x = 0; x < 3; x++) {
      float c = b->pos.Elements[x];
      if (c - b->radius - MODEL_BLOB_SMOOTH < model_blob_min.Elements[x]) {
        model_blob_min.Elements[x] = c - b->radius - MODEL_BLOB_SMOOTH;
      }
      if (c + b->radius + MODEL_BLOB_SMOOTH > model_blob_max.Elements[x]) {
        model_blob_max.Elements[x] = c + b->radius + MODEL_BLOB_SMOOTH;
      }
    }
  }
  float model_blob_size = 0.0f;
  for (int i = 0; i < 3; i++) {
    float diff = model_blob_max.Elements[i] - model_blob_min.Elements[i];
    if (diff > model_blob_size) {
      model_blob_size = diff;
    }
  }
  HMM_Vec3 model_blob_pos =
      HMM_MulV3F(HMM_AddV3(model_blob_min, model_blob_max), 0.5f);

  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, br->solids_ssbo);
  glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(model_blob_v4),
                  model_blob_v4);
  glBindImageTexture(0, br->sdf_mdl_tex, 0, GL_TRUE, 0, GL_WRITE_ONLY,
                     GL_RGBA8);
  glUseProgram(br->compute_program);
  glUniform1i(0, mdl->blob_count);
  glUniform3fv(1, 1, model_blob_pos.Elements);
  glUniform1f(2, model_blob_size);
  glUniform1i(3, BLOB_MODEL_SDF_RES);
  glUniform1f(4, MODEL_BLOB_SDF_MAX_DIST);
  glUniform1f(5, MODEL_BLOB_SMOOTH);
  glDispatchCompute(BLOB_MODEL_SDF_RES / BLOB_SDF_LOCAL_GROUP_COUNT,
                    BLOB_MODEL_SDF_RES / BLOB_SDF_LOCAL_GROUP_COUNT,
                    BLOB_MODEL_SDF_RES / BLOB_SDF_LOCAL_GROUP_COUNT);

  glUseProgram(br->raymarch_program);
  glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
  draw_sdf_cube(br, br->sdf_mdl_tex, &model_blob_pos, model_blob_size, trans,
                MODEL_BLOB_SDF_MAX_DIST);
}