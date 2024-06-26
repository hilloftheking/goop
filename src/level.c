#include <stdio.h>

#include "toml.h"

#include "blob.h"
#include "core.h"
#include "level.h"

static void level_load_fail(const char *msg) {
  fprintf(stderr, "Failed to load level: %s\n", msg);
  exit_fatal_error();
}

void level_load(BlobSim *bs, const char *data, int data_size) {
  char err_buff[128];

  toml_table_t *blvl =
      toml_parse((char *)data, data_size, err_buff, sizeof(err_buff));

  if (!blvl)
    level_load_fail(err_buff);

  toml_table_t *solids = toml_table_in(blvl, "solids");
  if (!solids)
    level_load_fail("No solids table");

  toml_array_t *radius_arr = toml_array_in(solids, "radius");
  if (!radius_arr)
    level_load_fail("No radius array");

  toml_array_t *pos_x_arr = toml_array_in(solids, "pos_x");
  if (!pos_x_arr)
    level_load_fail("No pos_x array");

  toml_array_t *pos_y_arr = toml_array_in(solids, "pos_y");
  if (!pos_y_arr)
    level_load_fail("No pos_y array");

  toml_array_t *pos_z_arr = toml_array_in(solids, "pos_z");
  if (!pos_z_arr)
    level_load_fail("No pos_z array");

  toml_array_t *mat_idx_arr = toml_array_in(solids, "mat_idx");
  if (!mat_idx_arr)
    level_load_fail("No mat_idx array");

  for (int i = 0;; i++) {
    toml_datum_t radius = toml_double_at(radius_arr, i);
    toml_datum_t pos_x = toml_double_at(pos_x_arr, i);
    toml_datum_t pos_y = toml_double_at(pos_y_arr, i);
    toml_datum_t pos_z = toml_double_at(pos_z_arr, i);
    toml_datum_t mat_idx = toml_int_at(mat_idx_arr, i);
    if (!radius.ok || !pos_x.ok || !pos_y.ok || !pos_z.ok || !mat_idx.ok)
      break;
    
    SolidBlob *b = solid_blob_create(bs);
    b->radius = (float)radius.u.d;
    b->pos.X = (float)pos_x.u.d;
    b->pos.Y = (float)pos_y.u.d;
    b->pos.Z = (float)pos_z.u.d;
    b->mat_idx = (int)mat_idx.u.i;
  }

  toml_free(blvl);
}