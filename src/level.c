#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "toml.h"

#include "HandmadeMath.h"

#include "blob.h"
#include "core.h"
#include "ecs.h"
#include "level.h"

#include "enemies/floater.h"

static void level_load_fail(const char *msg) {
  fprintf(stderr, "Failed to load level: %s\n", msg);
  exit_fatal_error();
}

static HMM_Vec3 parse_vec3(toml_array_t *arr) {
  toml_datum_t pos_x = toml_double_at(arr, 0);
  toml_datum_t pos_y = toml_double_at(arr, 1);
  toml_datum_t pos_z = toml_double_at(arr, 2);
  if (!pos_x.ok || !pos_y.ok || !pos_z.ok) {
    level_load_fail("Position does not have three numbers");
  }

  return HMM_V3((float)pos_x.u.d, (float)pos_y.u.d, (float)pos_z.u.d);
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

  toml_array_t *pos_arr = toml_array_in(solids, "pos");
  if (!pos_arr)
    level_load_fail("No pos array");

  toml_array_t *mat_idx_arr = toml_array_in(solids, "mat_idx");
  if (!mat_idx_arr)
    level_load_fail("No mat_idx array");

  for (int i = 0;; i++) {
    toml_datum_t radius = toml_double_at(radius_arr, i);
    toml_array_t *pos_xyz = toml_array_at(pos_arr, i);
    toml_datum_t mat_idx = toml_int_at(mat_idx_arr, i);
    if (!radius.ok || !pos_xyz || !mat_idx.ok)
      break;

    SolidBlob *b = solid_blob_create(bs);
    HMM_Vec3 pos = parse_vec3(pos_xyz);
    solid_blob_set_radius_pos(bs, b, (float)radius.u.d, &pos);
    b->mat_idx = (int)mat_idx.u.i;
  }

  toml_array_t *enemies_arr = toml_array_in(blvl, "enemies");

  if (enemies_arr) {
    for (int i = 0; i < toml_array_nelem(enemies_arr); i++) {
      toml_table_t *enemy = toml_table_at(enemies_arr, i);
      if (!enemy)
        break;

      toml_datum_t type = toml_string_in(enemy, "type");
      if (!type.ok)
        level_load_fail("Enemy does not have a type");

      toml_array_t *pos_xyz = toml_array_in(enemy, "pos");
      if (!pos_xyz)
        level_load_fail("Enemy does not have a position");

      HMM_Vec3 pos = parse_vec3(pos_xyz);

      printf("%s at (%f, %f, %f)\n", type.u.s, pos.X, pos.Y, pos.Z);

      if (strcmp("floater", type.u.s) == 0) {
        Entity enemy = floater_create();
        HMM_Mat4 *trans = entity_get_component(enemy, COMPONENT_TRANSFORM);
        trans->Columns[3].XYZ = pos;
      }

      free(type.u.s);
    }
  }

  toml_free(blvl);
}