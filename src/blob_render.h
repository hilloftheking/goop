#pragma once

#include <stdbool.h>

#include "linmath.h"

typedef struct BlobOtNode BlobOtNode;
typedef BlobOtNode *BlobOt;

typedef struct BlobRenderer {
  unsigned int vao, vbo, raymarch_program, compute_program, sdf_tex, blobs_tex,
      blob_ot_ssbo;
  int blob_ot_ssbo_size_bytes;

  mat4x4 cam_trans;
  float aspect_ratio;

  BlobOt blob_ot;
  vec4 *blobs_lerped;

  // If true, then recalculate the entire SDF image this frame
  bool compute_whole_sdf_this_frame;
} BlobRenderer;

typedef struct BlobSimulation BlobSimulation;

void blob_renderer_create(BlobRenderer *br);
void blob_renderer_destroy(BlobRenderer *br);

void blob_render(BlobRenderer *br, const BlobSimulation *bs);