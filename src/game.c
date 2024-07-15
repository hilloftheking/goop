#include "game.h"
#include "goop.h"
#include "level.h"
#include "player.h"
#include "resource.h"
#include "resource_load.h"

void game_init(GoopEngine *goop) {
  // Load test level
  {
    Resource blvl_rsrc;
    resource_load(&blvl_rsrc, IDS_TEST, "BLVL");
    level_load(&goop->bs, blvl_rsrc.data, blvl_rsrc.data_size);
    resource_destroy(&blvl_rsrc);
  }

  // Ground
  for (int i = 0; i < 512; i++) {
    HMM_Vec3 pos = {(rand_float() - 0.5f) * 32.0f, rand_float() * 0.5f - 1.0f,
                    (rand_float() - 0.5f) * 32.0f};
    SolidBlob *b = solid_blob_create(&goop->bs);
    if (b) {
      solid_blob_set_radius_pos(&goop->bs, b, 2.0f, &pos);
      b->mat_idx = 1;
    }
  }

  for (int i = 0; i < 32; i++) {
    HMM_Vec3 pos = {rand_float() - 0.5f, rand_float() - 0.5f,
                    -20.0f - i * 2.5f};
    SolidBlob *b = solid_blob_create(&goop->bs);
    if (b) {
      solid_blob_set_radius_pos(&goop->bs, b, 2.0f, &pos);
      b->mat_idx = 1;
    }
  }

  // Player

  Entity player_ent = player_create();
  {
    HMM_Mat4 *trans = entity_get_component(player_ent, COMPONENT_TRANSFORM);
    trans->Columns[3].Y = 6.0f;
  }
  global.player_ent = player_ent;

  global.cam_rot_x = -0.5f;
  global.cam_rot_y = HMM_PI32;
}
