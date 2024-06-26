#pragma once

#include <stdbool.h>

#include "HandmadeMath.h"

typedef struct BlobOtNode BlobOtNode;
typedef BlobOtNode *BlobOt;

typedef struct BlobRenderer {
  unsigned int raymarch_program, compute_program, sdf_tex, sdf_mdl_tex,
      blobs_ssbo, blob_ot_ssbo, water_tex, water_norm_tex;
  int blobs_ssbo_size_bytes, blob_ot_ssbo_size_bytes;

  HMM_Mat4 cam_trans, view_mat, proj_mat;

  float aspect_ratio;

  BlobOt blob_ot;
  HMM_Vec4 *blobs_lerped;
} BlobRenderer;

typedef struct BlobSim BlobSim;
typedef struct Model Model;

void blob_renderer_create(BlobRenderer *br);
void blob_renderer_destroy(BlobRenderer *br);

void blob_render_sim(BlobRenderer *br, const BlobSim *bs);

// This should be called after blob_render_sim()!!!!
void blob_render_mdl(BlobRenderer *br, const BlobSim *bs, const Model *mdl);