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

  br->blobs_ssbo_size_bytes =
      sizeof(HMM_Vec4) *
      (BLOB_SIM_MAX_SOLIDS + BLOB_SIM_MAX_LIQUIDS + BLOB_SIM_MAX_PROJECTILES);
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

  br->blob_ot.root_pos = HMM_V3(0, 0, 0);
  br->blob_ot.root_size = BLOB_ACTIVE_SIZE;
  blob_ot_create(&br->blob_ot);
  br->blob_ot.max_dist_to_leaf = BLOB_SDF_MAX_DIST;

  br->blobs_v4 = malloc(
      (BLOB_SIM_MAX_SOLIDS + BLOB_SIM_MAX_LIQUIDS + BLOB_SIM_MAX_PROJECTILES) *
      sizeof(*br->blobs_v4));

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

typedef struct RenderSolidData {
  BlobRenderer *br;
  const BlobSim *bs;
  int *ssbo_idx;
  bool checked[BLOB_SIM_MAX_SOLIDS];
} RenderSolidData;

static bool blob_render_sim_solids_ot_leaf(BlobOtEnumData *enum_data) {
  RenderSolidData *data = enum_data->user_data;
  for (int i = 0; i < enum_data->curr_leaf->leaf_blob_count; i++) {
    int bidx = enum_data->curr_leaf->offsets[i];
    if (!data->checked[bidx]) {
      data->checked[bidx] = true;

      const SolidBlob *b = fixed_array_get_const(&data->bs->solids, bidx);
      data->br->blobs_v4[*data->ssbo_idx].XYZ = b->pos;
      data->br->blobs_v4[*data->ssbo_idx].W =
          (float)((int)(b->radius * BLOB_RADIUS_MULT) * BLOB_MAT_COUNT +
                  b->mat_idx);

      blob_ot_insert(&data->br->blob_ot, &b->pos, b->radius, *data->ssbo_idx);
      (*data->ssbo_idx)++;
    }
  }

  return true;
}

void blob_render_sim(BlobRenderer *br, const BlobSim *bs) {
  blob_ot_reset(&br->blob_ot);
  br->blob_ot.root_pos = bs->active_pos;

  int ssbo_idx = 0;

  // Loop backwards so that new stuff is prioritized in the octree

  // Projectiles
  for (int i = bs->projectiles.count - 1; i >= 0; i--) {
    const Projectile *p = fixed_array_get_const(&bs->projectiles, i);

    br->blobs_v4[ssbo_idx].XYZ = p->pos;
    br->blobs_v4[ssbo_idx].W =
        (float)((int)(p->radius * BLOB_RADIUS_MULT) * BLOB_MAT_COUNT +
                p->mat_idx);

    blob_ot_insert(&br->blob_ot, &br->blobs_v4[ssbo_idx].XYZ, p->radius,
                   ssbo_idx);
    ssbo_idx++;
  }

  // Liquids
  for (int i = bs->liquids.count - 1; i >= 0; i--) {
    const LiquidBlob *b = fixed_array_get_const(&bs->liquids, i);

    br->blobs_v4[ssbo_idx].XYZ = b->pos;
    br->blobs_v4[ssbo_idx].W =
        (float)((int)(b->radius * BLOB_RADIUS_MULT) * BLOB_MAT_COUNT +
                b->mat_idx);

    blob_ot_insert(&br->blob_ot, &br->blobs_v4[ssbo_idx].XYZ, b->radius,
                   ssbo_idx);
    ssbo_idx++;
  }

  // Solids
  {
    RenderSolidData data = {0};
    data.br = br;
    data.bs = bs;
    data.ssbo_idx = &ssbo_idx;

    BlobOtEnumData enum_data;
    enum_data.bot = (BlobOt *)&bs->solid_ot;
    enum_data.shape_pos = bs->active_pos;
    enum_data.shape_size = BLOB_ACTIVE_SIZE;
    enum_data.user_data = &data;
    enum_data.callback = blob_render_sim_solids_ot_leaf;

    int before = ssbo_idx;
    blob_ot_enum_leaves_cube(&enum_data);
    printf("%d\n", ssbo_idx - before);
  }

  /*
  for (int i = bs->solids.count - 1; i >= 0; i--) {
    const SolidBlob *b = fixed_array_get_const(&bs->solids, i);

    br->blobs_v4[ssbo_idx].XYZ = b->pos;
    br->blobs_v4[ssbo_idx].W =
        (float)((int)(b->radius * BLOB_RADIUS_MULT) * BLOB_MAT_COUNT +
                b->mat_idx);

    blob_ot_insert(&br->blob_ot, &b->pos, b->radius, ssbo_idx);
    ssbo_idx++;
  }
  */

  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, br->blobs_ssbo);
  glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, br->blobs_ssbo_size_bytes,
                  br->blobs_v4);

  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, br->blob_ot_ssbo);
  glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, br->blob_ot_ssbo_size_bytes,
                  br->blob_ot.root);

  glBindImageTexture(0, br->sdf_tex, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA8);
  glUseProgram(br->compute_program);
  glUniform1i(0, -1); // Use the octree
  glUniform3fv(1, 1, br->blob_ot.root_pos.Elements);
  glUniform1f(2, BLOB_ACTIVE_SIZE);
  glUniform1i(3, BLOB_SIM_SDF_RES);
  glUniform1f(4, BLOB_SDF_MAX_DIST);
  glUniform1f(5, BLOB_SMOOTH);
  glDispatchCompute(BLOB_SIM_SDF_RES / BLOB_SDF_LOCAL_GROUP_COUNT,
                    BLOB_SIM_SDF_RES / BLOB_SDF_LOCAL_GROUP_COUNT,
                    BLOB_SIM_SDF_RES / BLOB_SDF_LOCAL_GROUP_COUNT);

  glBindVertexArray(cube_vao);

  glUseProgram(br->raymarch_program);
  glUniformMatrix4fv(1, 1, GL_FALSE, br->view_mat.Elements[0]);
  glUniformMatrix4fv(2, 1, GL_FALSE, br->proj_mat.Elements[0]);
  glUniform3fv(3, 1, br->cam_trans.Elements[3]);

  glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
  draw_sdf_cube(br, br->sdf_tex, &br->blob_ot.root_pos, BLOB_ACTIVE_SIZE, NULL,
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

  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, br->blobs_ssbo);
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