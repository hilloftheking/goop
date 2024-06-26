#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <glad/glad.h>

#include <GLFW/glfw3.h>

#include "HandmadeMath.h"

#include "blob.h"
#include "blob_render.h"
#include "blob_models.h"
#include "core.h"
#include "cube.h"
#include "level.h"
#include "skybox.h"

#include "resource.h"
#include "resource_load.h"

static void gl_debug_callback(GLenum source, GLenum type, GLuint id,
                              GLenum severity, GLsizei length,
                              const GLchar *message, const void *user_param) {
  fprintf(stderr, "[GL DEBUG] %s\n", message);
}

static float rand_float() { return ((float)rand() / (float)(RAND_MAX)); }

#define DELTA_MIN (1.0/30.0)

#define WIN_RES_X 1024
#define WIN_RES_Y 768

#define PLR_SPEED 4.0f
#define CAM_SENS 0.01f

#define SHOOT_CD 0.01

static HMM_Vec2 cam_rot;

static struct {
  BlobRenderer *br;
} cb_data;

static struct {
  bool player_dead;
  bool enemy_dead;
  Model *player_mdl;
  Model *enemy_mdl;
  BlobSim *blob_sim;
} global_data;

static void liquify_model(BlobSim *bs, const Model *mdl) {
  for (int i = 0; i < mdl->blob_count; i++) {
    HMM_Vec4 p = {0, 0, 0, 1};
    p.XYZ = mdl->blobs[i].pos;
    p = HMM_MulM4V4(mdl->transform, p);

    LiquidBlob *b = liquid_blob_create(bs);
    if (b) {
      b->type = LIQUID_BASE;
      b->radius = HMM_MAX(0.1f, mdl->blobs[i].radius);
      b->pos = p.XYZ;
      b->mat_idx = mdl->blobs[i].mat_idx;
      HMM_Vec3 force;
      for (int x = 0; x < 3; x++) {
        force.Elements[x] = (rand_float() - 0.5f) * 10.0f;
      }
      blob_sim_liquid_apply_force(bs, b, &force);
    }
  }
}

static void cursor_position_callback(GLFWwindow *window, double xpos,
                                     double ypos) {
  static double last_xpos = 0.0;
  static double last_ypos = 0.0;

  if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_1) == GLFW_PRESS) {
    cam_rot.X -= (float)(ypos - last_ypos) * CAM_SENS;
    cam_rot.Y -= (float)(xpos - last_xpos) * CAM_SENS;

    cam_rot.X = HMM_Clamp(HMM_PI32 * -0.45f, cam_rot.X, HMM_PI32 * 0.45f);
    cam_rot.Y = fmodf(cam_rot.Y, HMM_PI32 * 2.0f);
  }

  last_xpos = xpos;
  last_ypos = ypos;
}

static bool blob_sim_running = true;
static bool vsync_enabled = true;

static void key_callback(GLFWwindow *window, int key, int scancode, int action,
                         int mods) {
  if (action != GLFW_PRESS)
    return;

  switch (key) {
  case GLFW_KEY_P:
    blob_sim_running ^= true;
    break;
  case GLFW_KEY_V:
    vsync_enabled ^= true;
    glfwSwapInterval((int)vsync_enabled);
    break;
  case GLFW_KEY_K:
    if (!global_data.player_dead) {
      printf("Liquifying player\n");
      global_data.player_dead = true;
      liquify_model(global_data.blob_sim, global_data.player_mdl);
    }
    break;
  }
}

static void window_size_callback(GLFWwindow *window, int width, int height) {
  // Need size in pixels, not screen coordinates
  glfwGetFramebufferSize(window, &width, &height);
  glViewport(0, 0, width, height);
  if (cb_data.br) {
    cb_data.br->aspect_ratio = (float)width / height;
  }
}

static void glfw_fatal_error() {
  const char *err_desc;
  glfwGetError(&err_desc);
  fprintf(stderr, "[GLFW fatal] %s\n", err_desc);
  exit_fatal_error();
}

static void proj_callback(Projectile *p, uint64_t userdata) {
  if (HMM_LenV3(
          HMM_SubV3(p->pos, global_data.enemy_mdl->transform.Columns[3].XYZ)) <=
      0.5f + p->radius) {
    blob_sim_queue_delete(global_data.blob_sim, DELETE_PROJECTILE, p);

    if (!global_data.enemy_dead) {
      global_data.enemy_dead = true;
      printf("callback\n");

      liquify_model(global_data.blob_sim, global_data.enemy_mdl);
    }
  }
}

int main() {
  srand(time(NULL));

  if (!glfwInit())
    glfw_fatal_error();

  // OpenGL 4.3 for compute shaders

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
  glfwWindowHint(GLFW_DEPTH_BITS, 32);

  GLFWwindow *window =
      glfwCreateWindow(WIN_RES_X, WIN_RES_Y, "goop", NULL, NULL);
  if (!window)
    glfw_fatal_error();

  if (glfwRawMouseMotionSupported())
    glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
  else
    fprintf(stderr, "Raw mouse input not supported\n");

  glfwSetCursorPosCallback(window, cursor_position_callback);
  glfwSetKeyCallback(window, key_callback);
  glfwSetWindowSizeCallback(window, window_size_callback);

  glfwMakeContextCurrent(window);
  glfwSwapInterval((int)vsync_enabled);

  gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

  glEnable(GL_DEBUG_OUTPUT);
  glDebugMessageCallback(gl_debug_callback, NULL);
  glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE,
                        GL_DEBUG_SEVERITY_NOTIFICATION, 0, NULL, GL_FALSE);

  // Only back faces are rendered so that the camera can be inside of a bounding
  // cube
  glEnable(GL_CULL_FACE);
  glFrontFace(GL_CCW);
  glCullFace(GL_FRONT);

  glEnable(GL_DEPTH_TEST);
  glDepthRange(0.0, 1.0);

  BlobSim blob_sim;
  blob_sim_create(&blob_sim);
  global_data.blob_sim = &blob_sim;

  // Load test level
  {
    Resource blvl_rsrc;
    resource_load(&blvl_rsrc, IDS_TEST, "BLVL");
    level_load(&blob_sim, blvl_rsrc.data, blvl_rsrc.data_size);
    resource_destroy(&blvl_rsrc);
  }

  // Ground
  for (int i = 0; i < 512; i++) {
    HMM_Vec3 pos = {(rand_float() - 0.5f) * 12.0f, rand_float() * 0.5f,
                    (rand_float() - 0.5f) * 12.0f};
    SolidBlob *b = solid_blob_create(&blob_sim);
    if (b) {
      b->radius = 0.5f;
      b->pos = pos;
      b->mat_idx = 1;
    }
  }

  // Player model

  Model player_mdl;
  blob_mdl_create(&player_mdl, PLAYER_MDL, ARR_SIZE(PLAYER_MDL));
  player_mdl.transform.Columns[3].Y = 6.0f;
  global_data.player_mdl = &player_mdl;

  fixed_array_append(&blob_sim.collider_models, &(void *){&player_mdl});

  HMM_Quat player_quat = {0, 0, 0, 1};
  HMM_Vec3 player_vel = {0};
  HMM_Vec3 player_grav = {0, -9.81f, 0};

  // Angry face model

  Model angry_mdl;
  blob_mdl_create(&angry_mdl, ANGRY_MDL, ARR_SIZE(ANGRY_MDL));
  angry_mdl.transform.Columns[3].Y = 3.0f;
  global_data.enemy_mdl = &angry_mdl;

  fixed_array_append(&blob_sim.collider_models, &(void *){&angry_mdl});

  // Create cube buffers for blob renderer and skybox
  cube_create_buffers();

  Skybox skybox;
  skybox_create(&skybox);

  BlobRenderer blob_renderer;
  blob_renderer_create(&blob_renderer);
  blob_renderer.aspect_ratio = (float)WIN_RES_X / (float)WIN_RES_Y;
  cb_data.br = &blob_renderer;

  cam_rot.X = -0.5f;
  cam_rot.Y = (float)M_PI;

  uint64_t timer_freq = glfwGetTimerFrequency();
  uint64_t prev_timer = glfwGetTimerValue();

  int fps = 0;
  int frames_this_second = 0;
  double second_timer = 1.0;

  double blob_spawn_cd = 0.0;

  while (!glfwWindowShouldClose(window)) {
    uint64_t timer_val = glfwGetTimerValue();
    double delta = (timer_val - prev_timer) / (double)timer_freq;
    prev_timer = timer_val;

    frames_this_second++;
    second_timer -= delta;
    if (second_timer <= 0.0) {
      fps = frames_this_second;
      frames_this_second = 0;
      second_timer = 1.0;
    }

    // Avoid extremely high deltas
    delta = HMM_MIN(delta, DELTA_MIN);

    char win_title[100];
    snprintf(win_title, sizeof(win_title), "%d fps  -  %lf ms", fps,
             delta * 1000.0);
    glfwSetWindowTitle(window, win_title);

    glfwPollEvents();

    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_1) == GLFW_PRESS)
      glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    else
      glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

    // Simulate blobs

    if (blob_sim_running)
      blob_simulate(&blob_sim, delta);

    // Player movement/physics

    HMM_Mat4 *cam_trans = &blob_renderer.cam_trans;
    *cam_trans = HMM_M4D(1.0f);

    *cam_trans =
        HMM_MulM4(*cam_trans, HMM_Rotate_RH(cam_rot.Y, (HMM_Vec3){{0, 1, 0}}));
    *cam_trans =
        HMM_MulM4(*cam_trans, HMM_Rotate_RH(cam_rot.X, (HMM_Vec3){{1, 0, 0}}));

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

      float step = (HMM_PI32 * 4.0f) * (float)delta;
      player_quat = HMM_RotateTowardsQ(player_quat, step, HMM_M4ToQ_RH(rot_mat));
      rot_mat = HMM_QToM4(player_quat);

      player_mdl.transform.Columns[0] = rot_mat.Columns[0];
      player_mdl.transform.Columns[1] = rot_mat.Columns[1];
      player_mdl.transform.Columns[2] = rot_mat.Columns[2];
    }

    player_vel.X = 0;
    player_vel.Z = 0;
    player_vel = HMM_AddV3(player_vel, HMM_MulV3F(player_grav, (float)delta));
    player_vel = HMM_AddV3(player_vel, HMM_MulV3F(dir.XYZ, PLR_SPEED));
    HMM_Vec3 test_pos = HMM_AddV3(player_mdl.transform.Columns[3].XYZ,
                                  HMM_MulV3F(player_vel, (float)delta));
    HMM_Vec3 test_pos_with_offset = HMM_AddV3(test_pos, (HMM_Vec3){0, 0.5f, 0});
    HMM_Vec3 correction =
        blob_get_correction_from_solids(&blob_sim, &test_pos_with_offset, 0.5f);

    // Avoid slowly sliding off of mostly flat surfaces
    if (HMM_LenV3(player_vel) > 0.1f) {
      player_mdl.transform.Columns[3].XYZ = HMM_AddV3(test_pos, correction);
    }

    if (correction.Y && HMM_DotV3(HMM_NormV3(correction), (HMM_Vec3){0, 1, 0}) >
                            HMM_CosF(45.0f * HMM_DegToRad)) {
      player_vel.Y = 0.0f;

      if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
        player_vel.Y = 5.0f;
    }

    // Set camera position after moving player
    {
      HMM_Vec3 ro = HMM_AddV3(player_mdl.transform.Columns[3].XYZ,
                              (HMM_Vec3){0, 2.3f, 0});
      float cam_dist = 4.0f;
      HMM_Vec3 rd = HMM_MulV3F(cam_trans->Columns[2].XYZ, cam_dist);

      RaycastResult result;
      blob_sim_raycast(&result, &blob_sim, ro, rd);

      if (result.has_hit) {
        cam_dist = result.traveled * 0.7f;
      }

      cam_trans->Columns[3].XYZ =
          HMM_AddV3(ro, HMM_MulV3F(cam_trans->Columns[2].XYZ, cam_dist));
    }

    // Blinking

    const double BLINK_OCCUR = 2.0;
    const double BLINK_TIME = 0.3;
    const float EYE_RADIUS = 0.05f;
    static double t = 0.0;
    t += delta;
    player_mdl.blobs[8].radius = EYE_RADIUS;
    player_mdl.blobs[9].radius = EYE_RADIUS;
    if (t >= BLINK_OCCUR + BLINK_TIME) {
      t = 0.0;
    } else if (t >= BLINK_OCCUR) {
      float x = (float)(t - BLINK_OCCUR);
      float r =
          ((cosf(((2.0f * HMM_PI32) / (float)BLINK_TIME) * x) * 0.5f) + 0.5f) *
          EYE_RADIUS;
      player_mdl.blobs[8].radius = r;
      player_mdl.blobs[9].radius = r;
    }

    // Hold right click to create projectiles

    if (blob_spawn_cd > 0.0) {
      blob_spawn_cd -= delta;
    }

    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_2) == GLFW_PRESS &&
        blob_spawn_cd <= 0.0) {
      blob_spawn_cd = SHOOT_CD;

      float radius = 0.2f + rand_float() * 0.3f;

      HMM_Vec3 pos = HMM_AddV3(HMM_MulV3F(cam_trans->Columns[2].XYZ, -5.0f),
                               cam_trans->Columns[3].XYZ);
      pos.Y += 1.0f;

      HMM_Vec3 force = HMM_MulV3F(cam_trans->Columns[2].XYZ, -15.0f);
      force = HMM_AddV3(force, (HMM_Vec3){0, 5.0f, 0});

      Projectile *p = projectile_create(&blob_sim);
      if (p) {
        p->radius = radius;
        p->pos = pos;
        p->mat_idx = 5;
        p->vel = force;
        p->callback = proj_callback;
      }
    }

    // Enemy behavior

    if (!global_data.enemy_dead) {
      HMM_Vec3 player_pos = player_mdl.transform.Columns[3].XYZ;
      player_pos.Y += 2.0f;
      HMM_Vec3 player_dir =
          HMM_SubV3(player_pos, angry_mdl.transform.Columns[3].XYZ);
      player_dir = HMM_NormV3(player_dir);

      float yaw = -atan2f(player_dir.Z, player_dir.X) - HMM_PI32 * 0.5f;
      float pitch = asinf(player_dir.Y);

      HMM_Mat4 rot_mat = HMM_MulM4(HMM_Rotate_RH(yaw, HMM_V3(0, 1, 0)),
                                   HMM_Rotate_RH(pitch, HMM_V3(1, 0, 0)));
      HMM_Quat rot_quat = HMM_M4ToQ_RH(rot_mat);
      float step = (HMM_PI32 * 1.0f) * (float)delta;
      rot_quat =
          HMM_RotateTowardsQ(HMM_M4ToQ_RH(angry_mdl.transform), step, rot_quat);
      rot_mat = HMM_QToM4(rot_quat);
      rot_mat.Columns[3] = angry_mdl.transform.Columns[3];

      angry_mdl.transform = rot_mat;

      #define ENEMY_SHOOT_CD 2.0;
      static double enemy_shoot_timer = ENEMY_SHOOT_CD;

      enemy_shoot_timer -= delta;
      if (enemy_shoot_timer <= 0.0) {
        enemy_shoot_timer = ENEMY_SHOOT_CD;

        Projectile *p = projectile_create(&blob_sim);
        p->radius = 0.2f;
        p->mat_idx = 0;
        p->pos = angry_mdl.transform.Columns[3].XYZ;
        p->vel = HMM_MulV3F(angry_mdl.transform.Columns[2].XYZ, -20.0f);
        p->delete_timer = 1.0f;
      }
    }

    // Render scene

    blob_renderer.proj_mat = HMM_Perspective_RH_ZO(
        60.0f * HMM_DegToRad, blob_renderer.aspect_ratio, 0.1f, 100.0f);
    blob_renderer.view_mat = HMM_InvGeneralM4(*cam_trans);

    glClear(GL_DEPTH_BUFFER_BIT);

    skybox_draw(&skybox, &blob_renderer.view_mat, &blob_renderer.proj_mat);

    blob_render_sim(&blob_renderer, &blob_sim);
    if (!global_data.player_dead)
      blob_render_mdl(&blob_renderer, &blob_sim, &player_mdl);
    if (!global_data.enemy_dead)
      blob_render_mdl(&blob_renderer, &blob_sim, &angry_mdl);
    glfwSwapBuffers(window);
  }

  skybox_destroy(&skybox);

  blob_sim_destroy(&blob_sim);
  blob_renderer_destroy(&blob_renderer);

  glfwTerminate();
  return 0;
}
