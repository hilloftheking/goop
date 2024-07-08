#include "HandmadeMath.h"

#include "blob.h"
#include "blob_models.h"
#include "core.h"
#include "creature.h"
#include "floater.h"

#define SHOOT_CD 2.0

Entity floater_create() {
  Entity ent = entity_create();

  HMM_Mat4 *transform = entity_add_component(ent, COMPONENT_TRANSFORM);
  *transform = HMM_M4D(1.0f);

  Creature *creature = entity_add_component(ent, COMPONENT_CREATURE);
  creature->health = 20;
  creature->is_enemy = true;

  Floater *floater = entity_add_component(ent, COMPONENT_ENEMY_FLOATER);
  floater->shoot_timer = SHOOT_CD;

  Model *mdl = entity_add_component(ent, COMPONENT_MODEL);
  blob_mdl_create(mdl, ANGRY_MDL, ARR_SIZE(ANGRY_MDL));

  collider_model_add(global.blob_sim, ent);

  return ent;
}

void floater_process(Entity ent) {
  Floater *floater = entity_get_component(ent, COMPONENT_ENEMY_FLOATER);
  HMM_Mat4 *trans = entity_get_component(ent, COMPONENT_TRANSFORM);

  HMM_Mat4 *plr_trans =
      entity_get_component(global.player_ent, COMPONENT_TRANSFORM);
  HMM_Vec3 player_pos = plr_trans->Columns[3].XYZ;
  player_pos.Y += 2.0f;
  HMM_Vec3 player_dir = HMM_SubV3(player_pos, trans->Columns[3].XYZ);
  player_dir = HMM_NormV3(player_dir);

  float yaw = -atan2f(player_dir.Z, player_dir.X) - HMM_PI32 * 0.5f;
  float pitch = asinf(player_dir.Y);

  HMM_Mat4 rot_mat = HMM_MulM4(HMM_Rotate_RH(yaw, HMM_V3(0, 1, 0)),
                               HMM_Rotate_RH(pitch, HMM_V3(1, 0, 0)));
  HMM_Quat rot_quat = HMM_M4ToQ_RH(rot_mat);
  float step = (HMM_PI32 * 1.0f) * (float)global.curr_delta;
  rot_quat = HMM_RotateTowardsQ(HMM_M4ToQ_RH(*trans), step, rot_quat);
  rot_mat = HMM_QToM4(rot_quat);
  rot_mat.Columns[3] = trans->Columns[3];

  *trans = rot_mat;

  floater->shoot_timer -= global.curr_delta;
  if (floater->shoot_timer <= 0.0) {
    floater->shoot_timer = SHOOT_CD;

    LiquidBlob *p = projectile_create(global.blob_sim);
    if (p) {
      p->mat_idx = 0;
      p->vel = HMM_MulV3F(trans->Columns[2].XYZ, -20.0f);
      p->proj.delete_timer = 1.0f;
      liquid_blob_set_radius_pos(global.blob_sim, p, 0.2f,
                                 &trans->Columns[3].XYZ);
    }
  }
}