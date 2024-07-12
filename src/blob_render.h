#pragma once

#include <stdbool.h>

#include "HandmadeMath.h"

#include "blob.h"

typedef struct BlobRenderer {
  unsigned int raymarch_program, compute_program, sdf_sim_solid_tex,
      sdf_sim_liquid_tex, sdf_mdl_tex, solids_ssbo, liquids_ssbo, solid_ot_ssbo,
      liquid_ot_ssbo, water_tex, water_norm_tex, screen_fbo, screen_color_tex,
      screen_depth_stencil_tex;
  int solids_ssbo_size_bytes, liquids_ssbo_size_bytes, solid_ot_ssbo_size_bytes,
      liquid_ot_ssbo_size_bytes;

  HMM_Mat4 cam_trans, view_mat, proj_mat;

  HMM_Vec4 *solids_v4;
  HMM_Vec4 *liquids_v4;
} BlobRenderer;

typedef struct BlobSim BlobSim;
typedef struct Model Model;

void blob_renderer_create(BlobRenderer *br);
void blob_renderer_destroy(BlobRenderer *br);

void blob_renderer_update_framebuffer(BlobRenderer *br);

// Call this at the start of a frame
void blob_render_start(BlobRenderer *br);

// This should be called last
void blob_render_sim(BlobRenderer *br, const BlobSim *bs);

void blob_render_mdl(BlobRenderer *br, const BlobSim *bs, const Model *mdl,
                     const HMM_Mat4 *trans);
