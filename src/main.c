#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glad/glad.h>

#include <GLFW/glfw3.h>

#include "HandmadeMath.h"

#include "blob_models.h"
#include "creature.h"
#include "goop.h"
#include "level.h"
#include "player.h"

#include "resource.h"
#include "resource_load.h"

#include "enemies/floater.h"

#define DELTA_MIN (1.0/30.0)

#define CAM_SENS 0.01f

static void cursor_position_callback(GLFWwindow *window, double xpos,
                                     double ypos) {
  static double last_xpos = 0.0;
  static double last_ypos = 0.0;

  if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_1) == GLFW_PRESS) {
    global.cam_rot_x -= (float)(ypos - last_ypos) * CAM_SENS;
    global.cam_rot_y -= (float)(xpos - last_xpos) * CAM_SENS;

    global.cam_rot_x =
        HMM_Clamp(HMM_PI32 * -0.45f, global.cam_rot_x, HMM_PI32 * 0.45f);
    global.cam_rot_y = fmodf(global.cam_rot_y, HMM_PI32 * 2.0f);
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
  }
}

static void window_size_callback(GLFWwindow *window, int width, int height) {
  // Need size in pixels, not screen coordinates
  glfwGetFramebufferSize(window, &width, &height);
  glViewport(0, 0, width, height);
  global.win_width = width;
  global.win_height = height;
  if (global.blob_renderer)
    blob_renderer_update_framebuffer(global.blob_renderer);
}

int main() {
  GoopEngine goop;
  goop_create(&goop);

  glfwSetCursorPosCallback(goop.window, cursor_position_callback);
  glfwSetKeyCallback(goop.window, key_callback);
  glfwSetWindowSizeCallback(goop.window, window_size_callback);

  // Load test level
  {
    Resource blvl_rsrc;
    resource_load(&blvl_rsrc, IDS_TEST, "BLVL");
    level_load(&goop.bs, blvl_rsrc.data, blvl_rsrc.data_size);
    resource_destroy(&blvl_rsrc);
  }

  // Ground
  for (int i = 0; i < 512; i++) {
    HMM_Vec3 pos = {(rand_float() - 0.5f) * 32.0f, rand_float() * 0.5f - 1.0f,
                    (rand_float() - 0.5f) * 32.0f};
    SolidBlob *b = solid_blob_create(&goop.bs);
    if (b) {
      solid_blob_set_radius_pos(&goop.bs, b, 2.0f, &pos);
      b->mat_idx = 1;
    }
  }

  for (int i = 0; i < 32; i++) {
    HMM_Vec3 pos = {rand_float() - 0.5f, rand_float() - 0.5f,
                    -20.0f - i * 2.5f};
    SolidBlob *b = solid_blob_create(&goop.bs);
    if (b) {
      solid_blob_set_radius_pos(&goop.bs, b, 2.0f, &pos);
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

  uint64_t timer_freq = glfwGetTimerFrequency();
  uint64_t prev_timer = glfwGetTimerValue();

  int fps = 0;
  int frames_this_second = 0;
  double second_timer = 1.0;

  while (!glfwWindowShouldClose(goop.window)) {
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

    glfwPollEvents();

    if (glfwGetMouseButton(goop.window, GLFW_MOUSE_BUTTON_1) == GLFW_PRESS)
      glfwSetInputMode(goop.window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    else
      glfwSetInputMode(goop.window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

    // Avoid extremely high deltas
    delta = HMM_MIN(delta, DELTA_MIN);
    global.curr_delta = delta;

    // Simulate blobs

    if (blob_sim_running)
      blob_simulate(&goop.bs, delta);

    // Process player (also handles camera)

    player_process(player_ent);

    // Enemy behavior

    for (EntityComponent *ec = component_begin(COMPONENT_ENEMY_FLOATER);
         ec != component_end(COMPONENT_ENEMY_FLOATER);
         ec = component_next(COMPONENT_ENEMY_FLOATER, ec)) {
      floater_process(ec->entity);
    }

    // Render scene

    float aspect_ratio = (float)global.win_width / (float)global.win_height;
    goop.br.proj_mat =
        HMM_Perspective_RH_ZO(60.0f * HMM_DegToRad, aspect_ratio, 0.1f, 100.0f);
    goop.br.view_mat = HMM_InvGeneralM4(goop.br.cam_trans);

    blob_render_start(&goop.br);

    skybox_draw(&goop.skybox, &goop.br.view_mat, &goop.br.proj_mat);

    HMM_Mat4 *plr_trans =
        entity_get_component(player_ent, COMPONENT_TRANSFORM);
    for (EntityComponent *ent_model = component_begin(COMPONENT_MODEL);
         ent_model != component_end(COMPONENT_MODEL);
         ent_model = component_next(COMPONENT_MODEL, ent_model)) {
      Model *mdl = ent_model->component;
      HMM_Mat4 *trans =
          entity_get_component(ent_model->entity, COMPONENT_TRANSFORM);
      if (HMM_LenV3(HMM_SubV3(trans->Columns[3].XYZ,
                              plr_trans->Columns[3].XYZ)) < BLOB_ACTIVE_SIZE) {
        blob_render_mdl(&goop.br, &goop.bs, mdl, trans);
      }
    }

    blob_render_sim(&goop.br, &goop.bs);

    char perf_text[256];
    snprintf(perf_text, sizeof(perf_text),
             "%d fps\n%.3f ms\n%d solids\n%d liquids", fps, delta * 1000.0,
             goop.bs.solids.count, goop.bs.liquids.count);
    text_render(&goop.txtr, perf_text, 32, 32);

    glfwSwapBuffers(goop.window);
  }

  goop_destroy(&goop);
  return 0;
}
