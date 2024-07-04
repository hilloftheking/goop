#include <math.h>
#include <stdbool.h>
#include <stddef.h>
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
#include "creature.h"
#include "cube.h"
#include "ecs.h"
#include "level.h"
#include "player.h"
#include "skybox.h"

#include "resource.h"
#include "resource_load.h"

#include "enemies/floater.h"

static void gl_debug_callback(GLenum source, GLenum type, GLuint id,
                              GLenum severity, GLsizei length,
                              const GLchar *message, const void *user_param) {
  fprintf(stderr, "[GL DEBUG] %s\n", message);
}

#define DELTA_MIN (1.0/30.0)

#define WIN_RES_X 1024
#define WIN_RES_Y 768

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
  if (global.blob_renderer) {
    global.blob_renderer->aspect_ratio = (float)width / height;
  }
}

static void glfw_fatal_error() {
  const char *err_desc;
  glfwGetError(&err_desc);
  fprintf(stderr, "[GLFW fatal] %s\n", err_desc);
  exit_fatal_error();
}

int main() {
  ecs_register_component(COMPONENT_TRANSFORM, sizeof(HMM_Mat4));
  ecs_register_component(COMPONENT_MODEL, sizeof(Model));
  ecs_register_component(COMPONENT_CREATURE, sizeof(Creature));
  ecs_register_component(COMPONENT_PLAYER, sizeof(Player));
  ecs_register_component(COMPONENT_ENEMY_FLOATER, sizeof(Floater));

  unsigned int seed = time(NULL);
  srand(seed);

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
  global.window = window;

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
  global.blob_sim = &blob_sim;

  // Load test level
  {
    Resource blvl_rsrc;
    resource_load(&blvl_rsrc, IDS_TEST, "BLVL");
    level_load(&blob_sim, blvl_rsrc.data, blvl_rsrc.data_size);
    resource_destroy(&blvl_rsrc);
  }

  // Ground
  for (int i = 0; i < 512; i++) {
    HMM_Vec3 pos = {(rand_float() - 0.5f) * 32.0f, rand_float() * 0.5f - 1.0f,
                    (rand_float() - 0.5f) * 32.0f};
    SolidBlob *b = solid_blob_create(&blob_sim);
    if (b) {
      solid_blob_set_radius_pos(&blob_sim, b, 2.0f, &pos);
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

  // Create cube buffers for blob renderer and skybox
  cube_create_buffers();

  Skybox skybox;
  skybox_create(&skybox);

  BlobRenderer blob_renderer;
  blob_renderer_create(&blob_renderer);
  blob_renderer.aspect_ratio = (float)WIN_RES_X / (float)WIN_RES_Y;
  global.blob_renderer = &blob_renderer;

  global.cam_rot_x = -0.5f;
  global.cam_rot_y = HMM_PI32;

  uint64_t timer_freq = glfwGetTimerFrequency();
  uint64_t prev_timer = glfwGetTimerValue();

  int fps = 0;
  int frames_this_second = 0;
  double second_timer = 1.0;

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

    char win_title[100];
    snprintf(win_title, sizeof(win_title), "%d fps  -  %lf ms", fps,
             delta * 1000.0);
    glfwSetWindowTitle(window, win_title);

    glfwPollEvents();

    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_1) == GLFW_PRESS)
      glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    else
      glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

    // Avoid extremely high deltas
    delta = HMM_MIN(delta, DELTA_MIN);
    global.curr_delta = delta;

    // Simulate blobs

    if (blob_sim_running)
      blob_simulate(&blob_sim, delta);

    // Process player (also handles camera)

    player_process(player_ent);

    // Enemy behavior

    for (EntityComponent *ec = component_begin(COMPONENT_ENEMY_FLOATER);
         ec != component_end(COMPONENT_ENEMY_FLOATER);
         ec = component_next(COMPONENT_ENEMY_FLOATER, ec)) {
      floater_process(ec->entity);
    }

    // Render scene

    blob_renderer.proj_mat = HMM_Perspective_RH_ZO(
        60.0f * HMM_DegToRad, blob_renderer.aspect_ratio, 0.1f, 100.0f);
    blob_renderer.view_mat = HMM_InvGeneralM4(blob_renderer.cam_trans);

    glClear(GL_DEPTH_BUFFER_BIT);

    skybox_draw(&skybox, &blob_renderer.view_mat, &blob_renderer.proj_mat);

    blob_render_sim(&blob_renderer, &blob_sim);

    for (EntityComponent *ent_model = component_begin(COMPONENT_MODEL);
         ent_model != component_end(COMPONENT_MODEL);
         ent_model = component_next(COMPONENT_MODEL, ent_model)) {
      Model *mdl = ent_model->component;
      HMM_Mat4 *trans =
          entity_get_component(ent_model->entity, COMPONENT_TRANSFORM);
      blob_render_mdl(&blob_renderer, &blob_sim, mdl, trans);
    }

    glfwSwapBuffers(window);
  }

  skybox_destroy(&skybox);

  blob_sim_destroy(&blob_sim);
  blob_renderer_destroy(&blob_renderer);

  glfwTerminate();
  return 0;
}
