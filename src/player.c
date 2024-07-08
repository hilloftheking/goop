#include <GLFW/glfw3.h>

#include "blob.h"
#include "blob_models.h"
#include "blob_render.h"
#include "creature.h"
#include "ecs.h"
#include "player.h"

#include "enemies/floater.h"

#define PLR_SPEED 4.0f
#define SHOOT_CD 0.01

Entity player_create() {
  Entity ent = entity_create();

  HMM_Mat4 *transform = entity_add_component(ent, COMPONENT_TRANSFORM);
  *transform = HMM_M4D(1.0f);

  Creature *creature = entity_add_component(ent, COMPONENT_CREATURE);
  creature->health = 20;
  creature->is_enemy = false;

  Player *player = entity_add_component(ent, COMPONENT_PLAYER);
  player->shoot_timer = SHOOT_CD;
  player->blink_timer = 0.0;
  player->quat = HMM_Q(0, 0, 0, 1);
  player->vel = HMM_V3(0, 0, 0);
  player->grav = HMM_V3(0, -9.81f, 0);

  Model *mdl = entity_add_component(ent, COMPONENT_MODEL);
  blob_mdl_create(mdl, PLAYER_MDL, ARR_SIZE(PLAYER_MDL));

  return ent;
}

static void liquify_model(Entity e) {
  HMM_Mat4 *trans = entity_get_component(e, COMPONENT_TRANSFORM);
  Model *mdl = entity_get_component(e, COMPONENT_MODEL);

  for (int i = 0; i < mdl->blob_count; i++) {
    HMM_Vec4 p = {0, 0, 0, 1};
    p.XYZ = mdl->blobs[i].pos;
    p = HMM_MulM4V4(*trans, p);

    LiquidBlob *b = liquid_blob_create(global.blob_sim);
    if (b) {
      b->type = LIQUID_BASE;
      b->mat_idx = mdl->blobs[i].mat_idx;
      liquid_blob_set_radius_pos(global.blob_sim, b,
                                 HMM_MAX(0.2f, mdl->blobs[i].radius), &p.XYZ);
      HMM_Vec3 force;
      for (int x = 0; x < 3; x++) {
        force.Elements[x] = (rand_float() - 0.5f) * 10.0f;
      }
      force.Y += 8.0f;
      b->vel = force;
    }
  }
}

static void proj_callback(LiquidBlob *p, ColliderModel *col_mdl) {
  Entity ent = col_mdl->ent;
  Creature *creature = entity_get_component(ent, COMPONENT_CREATURE);

  if (!creature || creature->health <= 0 || !creature->is_enemy) {
    return;
  }

  // Blood
  for (int i = 0; i < 4; i++) {
    LiquidBlob *b = liquid_blob_create(global.blob_sim);
    if (b) {
      b->type = LIQUID_BASE;
      b->mat_idx = 0;
      liquid_blob_set_radius_pos(global.blob_sim, b, 0.2f, &p->pos);

      HMM_Vec3 force;
      for (int x = 0; x < 3; x++) {
        force.Elements[x] = (rand_float() - 0.5f) * 30.0f;
      }
      b->vel = force;
    }
  }

  creature->health -= 1;
  blob_sim_queue_delete(global.blob_sim, DELETE_LIQUID, p);

  if (creature->health <= 0) {
    Model *mdl = entity_get_component(ent, COMPONENT_MODEL);
    liquify_model(ent);
    blob_sim_queue_delete(global.blob_sim, DELETE_COLLIDER_MODEL, col_mdl);

    blob_mdl_destroy(mdl);
    entity_destroy(ent);
  }
}

void player_process(Entity ent) {
  Player *player = entity_get_component(ent, COMPONENT_PLAYER);
  HMM_Mat4 *trans = entity_get_component(ent, COMPONENT_TRANSFORM);
  Model *mdl = entity_get_component(ent, COMPONENT_MODEL);

  // Camera rotation

  HMM_Mat4 *cam_trans = &global.blob_renderer->cam_trans;
  *cam_trans = HMM_M4D(1.0f);

  *cam_trans = HMM_MulM4(
      *cam_trans, HMM_Rotate_RH(global.cam_rot_y, (HMM_Vec3){{0, 1, 0}}));
  *cam_trans = HMM_MulM4(
      *cam_trans, HMM_Rotate_RH(global.cam_rot_x, (HMM_Vec3){{1, 0, 0}}));

  // Movement/physics

  GLFWwindow *window = global.window;
  bool w_press = glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS;
  bool s_press = glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS;
  bool a_press = glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS;
  bool d_press = glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS;
  HMM_Vec4 dir = {0};
  dir.X = (float)(d_press - a_press);
  dir.Z = (float)(s_press - w_press);
  if (dir.X || dir.Z) {
    dir = HMM_MulM4V4(*cam_trans, dir);
    dir.Y = 0;
    dir.W = 0;
    dir = HMM_NormV4(dir);

    HMM_Mat4 rot_mat = HMM_M4D(1.0f);
    rot_mat.Columns[2].XYZ = HMM_MulV3F(dir.XYZ, -1.0f);
    rot_mat.Columns[1].XYZ = (HMM_Vec3){0, 1, 0};
    rot_mat.Columns[0].XYZ =
        HMM_Cross(rot_mat.Columns[1].XYZ, rot_mat.Columns[2].XYZ);

    float step = (HMM_PI32 * 4.0f) * (float)global.curr_delta;
    player->quat =
        HMM_RotateTowardsQ(player->quat, step, HMM_M4ToQ_RH(rot_mat));
    rot_mat = HMM_QToM4(player->quat);

    trans->Columns[0] = rot_mat.Columns[0];
    trans->Columns[1] = rot_mat.Columns[1];
    trans->Columns[2] = rot_mat.Columns[2];
  }

  player->vel.X = 0;
  player->vel.Z = 0;
  player->vel = HMM_AddV3(player->vel,
                          HMM_MulV3F(player->grav, (float)global.curr_delta));
  player->vel = HMM_AddV3(player->vel, HMM_MulV3F(dir.XYZ, PLR_SPEED));
  HMM_Vec3 test_pos = HMM_AddV3(
      trans->Columns[3].XYZ, HMM_MulV3F(player->vel, (float)global.curr_delta));
  HMM_Vec3 test_pos_with_offset = HMM_AddV3(test_pos, (HMM_Vec3){0, 0.5f, 0});
  HMM_Vec3 correction = blob_get_correction_from_solids(
      global.blob_sim, &test_pos_with_offset, 0.5f);

  // Avoid slowly sliding off of mostly flat surfaces
  if (HMM_LenV3(player->vel) > 0.1f) {
    trans->Columns[3].XYZ = HMM_AddV3(test_pos, correction);
  }

  if (correction.Y && HMM_DotV3(HMM_NormV3(correction), (HMM_Vec3){0, 1, 0}) >
                          HMM_CosF(45.0f * HMM_DegToRad)) {
    player->vel.Y = 0.0f;
  }

  if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
    player->vel.Y = 5.0f;

  // Blinking

  const double BLINK_OCCUR = 2.0;
  const double BLINK_TIME = 0.3;
  const float EYE_RADIUS = 0.05f;
  player->blink_timer += global.curr_delta;
  mdl->blobs[8].radius = EYE_RADIUS;
  mdl->blobs[9].radius = EYE_RADIUS;
  if (player->blink_timer >= BLINK_OCCUR + BLINK_TIME) {
    player->blink_timer = 0.0;
  } else if (player->blink_timer >= BLINK_OCCUR) {
    float x = (float)(player->blink_timer - BLINK_OCCUR);
    float r =
        ((cosf(((2.0f * HMM_PI32) / (float)BLINK_TIME) * x) * 0.5f) + 0.5f) *
        EYE_RADIUS;
    mdl->blobs[8].radius = r;
    mdl->blobs[9].radius = r;
  }

  // Now set the camera position

  {
    HMM_Vec3 ro = HMM_AddV3(trans->Columns[3].XYZ, (HMM_Vec3){0, 2.3f, 0});
    float cam_dist = 4.0f;
    HMM_Vec3 rd = HMM_MulV3F(cam_trans->Columns[2].XYZ, cam_dist);

    RaycastResult result;
    blob_sim_raycast(&result, global.blob_sim, ro, rd);

    if (result.has_hit) {
      cam_dist = result.traveled * 0.7f;
    }

    cam_trans->Columns[3].XYZ =
        HMM_AddV3(ro, HMM_MulV3F(cam_trans->Columns[2].XYZ, cam_dist));
  }

  // Hold right click to create projectiles

  if (player->shoot_timer > 0.0) {
    player->shoot_timer -= global.curr_delta;
  }

  if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_2) == GLFW_PRESS &&
      player->shoot_timer <= 0.0) {
    player->shoot_timer = SHOOT_CD;

    const float radius = 0.2f;

    HMM_Vec3 proj_dir = cam_trans->Columns[2].XYZ;
    proj_dir = HMM_RotateV3AxisAngle_RH(proj_dir, cam_trans->Columns[1].XYZ,
                                        (rand_float() - 0.5f) * 0.1f);
    proj_dir = HMM_RotateV3AxisAngle_RH(proj_dir, cam_trans->Columns[0].XYZ,
                                        (rand_float() - 0.5f) * 0.1f);

    HMM_Vec3 pos =
        HMM_AddV3(HMM_MulV3F(proj_dir, -5.0f), cam_trans->Columns[3].XYZ);
    pos.Y += 1.0f;

    HMM_Vec3 force = HMM_MulV3F(proj_dir, -15.0f);
    force = HMM_AddV3(force, (HMM_Vec3){0, 5.0f, 0});

    LiquidBlob *p = projectile_create(global.blob_sim);
    if (p) {
      p->mat_idx = 6;
      p->vel = force;
      p->proj.callback = proj_callback;
      liquid_blob_set_radius_pos(global.blob_sim, p, radius, &pos);
      liquid_blob_delete_after(global.blob_sim, p, 2.0);
    }
  }

  // Update blob simulation active cube position
  global.blob_sim->active_pos = trans->Columns[3].XYZ;
}