#pragma once

#include <stdbool.h>

#include "linmath.h"

typedef struct BlobOtNode BlobOtNode;
typedef BlobOtNode *BlobOt;

typedef struct BlobRenderer {
  unsigned int raymarch_program, compute_program, sdf_tex, sdf_mdl_tex,
      blobs_ssbo, blob_ot_ssbo;
  int blobs_ssbo_size_bytes, blob_ot_ssbo_size_bytes;

  mat4x4 cam_trans;
  mat4x4 view_mat;
  mat4x4 proj_mat;

  float aspect_ratio;

  BlobOt blob_ot;
  vec4 *blobs_lerped;
} BlobRenderer;

typedef struct BlobSimulation BlobSimulation;

void blob_renderer_create(BlobRenderer *br);
void blob_renderer_destroy(BlobRenderer *br);

void blob_render_sim(BlobRenderer *br, const BlobSimulation *bs);

// This should be called after blob_render_sim()!!!!
void blob_render_mdl(BlobRenderer *br, const BlobSimulation *bs,
                     int mdl_blob_idx, int mdl_blob_count);