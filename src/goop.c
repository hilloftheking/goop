#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <glad/glad.h>

#include <GLFW/glfw3.h>

#include "core.h"
#include "ecs.h"
#include "goop.h"
#include "primitives.h"

// These are currently needed to register/process ECS components
#include "creature.h"
#include "editor.h"
#include "enemies/floater.h"
#include "player.h"

#define WIN_RES_X 1024
#define WIN_RES_Y 768
#define DELTA_MIN (1.0 / 30.0)
#define CAM_SENS 0.01f

static void glfw_fatal_error() {
  const char *err_desc;
  glfwGetError(&err_desc);
  fprintf(stderr, "[GLFW fatal] %s\n", err_desc);
  exit_fatal_error();
}

static void gl_debug_callback(GLenum source, GLenum type, GLuint id,
                              GLenum severity, GLsizei length,
                              const GLchar *message, const void *user_param) {
  fprintf(stderr, "[GL DEBUG] %s\n", message);
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

static void emit_input_event(InputEvent *event) {
  for (int i = 0; i < component_get_count(COMPONENT_INPUT_HANDLER); i++) {
    EntityComponent *ec = component_get_from_idx(COMPONENT_INPUT_HANDLER, i);
    InputHandler *handler = (InputHandler *)ec->component;
    if (handler->mask & event->type) {
      handler->callback(ec->entity, event);
    }
  }
}

static void cursor_pos_callback(GLFWwindow *window, double xpos, double ypos) {
  static double last_xpos = 0.0;
  static double last_ypos = 0.0;

  double relx = xpos - last_xpos;
  double rely = ypos - last_ypos;

  if (global.mouse_captured) {
    global.cam_rot_x -= (float)rely * CAM_SENS;
    global.cam_rot_y -= (float)relx * CAM_SENS;

    global.cam_rot_x =
        HMM_Clamp(HMM_PI32 * -0.45f, global.cam_rot_x, HMM_PI32 * 0.45f);
    global.cam_rot_y = fmodf(global.cam_rot_y, HMM_PI32 * 2.0f);
  }

  InputEvent event;
  event.type = INPUT_MOUSE_MOTION;
  event.mouse_motion.x = xpos;
  event.mouse_motion.y = ypos;
  event.mouse_motion.relx = relx;
  event.mouse_motion.rely = rely;
  emit_input_event(&event);

  last_xpos = xpos;
  last_ypos = ypos;
}

static void mouse_button_callback(GLFWwindow *window, int button, int action,
                                  int mods) {
  InputEvent event;
  event.type = INPUT_MOUSE_BUTTON;
  event.mouse_button.button = button;
  event.mouse_button.pressed = action;
  emit_input_event(&event);
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

  InputEvent event;
  event.type = INPUT_KEY;
  event.key.key = key;
  event.key.mods = mods;
  event.key.pressed = action == GLFW_PRESS;
  emit_input_event(&event);
}

void goop_create(GoopEngine *goop) {
  ecs_register_component(COMPONENT_INPUT_HANDLER, sizeof(InputHandler));
  ecs_register_component(COMPONENT_TEXT_BOX, sizeof(TextBox));
  ecs_register_component(COMPONENT_TRANSFORM, sizeof(HMM_Mat4));
  ecs_register_component(COMPONENT_MODEL, sizeof(Model));
  ecs_register_component(COMPONENT_CREATURE, sizeof(Creature));
  ecs_register_component(COMPONENT_PLAYER, sizeof(Player));
  ecs_register_component(COMPONENT_ENEMY_FLOATER, sizeof(Floater));
  ecs_register_component(COMPONENT_EDITOR, sizeof(Editor));

  unsigned int seed = time(NULL);
  srand(seed);

  if (!glfwInit())
    glfw_fatal_error();

  // OpenGL 4.3 for compute shaders

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);

  global.win_width = WIN_RES_X;
  global.win_height = WIN_RES_Y;
  goop->window = glfwCreateWindow(WIN_RES_X, WIN_RES_Y, "goop", NULL, NULL);
  if (!goop->window)
    glfw_fatal_error();
  global.window = goop->window;

  if (glfwRawMouseMotionSupported())
    glfwSetInputMode(goop->window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
  else
    fprintf(stderr, "Raw mouse input not supported\n");

  glfwSetWindowSizeCallback(goop->window, window_size_callback);
  glfwSetCursorPosCallback(goop->window, cursor_pos_callback);
  glfwSetMouseButtonCallback(goop->window, mouse_button_callback);
  glfwSetKeyCallback(goop->window, key_callback);

  glfwMakeContextCurrent(goop->window);
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

  blob_sim_create(&goop->bs);
  global.blob_sim = &goop->bs;

  // Create primitive buffers before creating renderers
  primitives_create_buffers();

  skybox_create(&goop->skybox);

  blob_renderer_create(&goop->br);
  global.blob_renderer = &goop->br;

  text_renderer_create(&goop->txtr, "C:\\Windows\\Fonts\\times.ttf", 24);
}

void goop_destroy(GoopEngine *goop) {
  skybox_destroy(&goop->skybox);

  blob_sim_destroy(&goop->bs);
  blob_renderer_destroy(&goop->br);

  text_renderer_destroy(&goop->txtr);

  glfwTerminate();
}

void goop_main_loop(GoopEngine *goop) {
  uint64_t timer_freq = glfwGetTimerFrequency();
  uint64_t prev_timer = glfwGetTimerValue();

  int fps = 0;
  int frames_this_second = 0;
  double second_timer = 1.0;

  while (!glfwWindowShouldClose(goop->window)) {
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

    if (global.mouse_captured) {
      glfwSetInputMode(goop->window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    } else {
      glfwSetInputMode(goop->window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }

    // Avoid extremely high deltas
    double actual_delta = delta;
    delta = HMM_MIN(delta, DELTA_MIN);
    global.curr_delta = delta;

    // Simulate blobs

    if (blob_sim_running)
      blob_simulate(&goop->bs, delta);

    // Process player (also handles camera)

    for (int i = 0; i < component_get_count(COMPONENT_PLAYER); i++) {
      player_process(component_get_from_idx(COMPONENT_PLAYER, i)->entity);
    }

    // Editor

    for (int i = 0; i < component_get_count(COMPONENT_EDITOR); i++) {
      editor_process(component_get_from_idx(COMPONENT_EDITOR, i)->entity);
    }

    // Enemy behavior

    for (int i = 0; i < component_get_count(COMPONENT_ENEMY_FLOATER); i++) {
      floater_process(
          component_get_from_idx(COMPONENT_ENEMY_FLOATER, i)->entity);
    }

    // Render scene

    float aspect_ratio = (float)global.win_width / (float)global.win_height;
    goop->br.proj_mat =
        HMM_Perspective_RH_ZO(60.0f * HMM_DegToRad, aspect_ratio, 0.1f, 100.0f);
    goop->br.view_mat = HMM_InvGeneralM4(goop->br.cam_trans);

    blob_render_start(&goop->br);

    skybox_draw(&goop->skybox, &goop->br.view_mat, &goop->br.proj_mat);

    for (int i = 0; i < component_get_count(COMPONENT_MODEL); i++) {
      EntityComponent *ec = component_get_from_idx(COMPONENT_MODEL, i);
      Model *mdl = (Model *)ec->component;
      HMM_Mat4 *trans = entity_get_component(ec->entity, COMPONENT_TRANSFORM);
      blob_render_mdl(&goop->br, &goop->bs, mdl, trans);
    }

    blob_render_sim(&goop->br, &goop->bs);

    int mem_bytes = 0;
    mem_bytes += goop->bs.solid_ot.size_int * 4;
    mem_bytes += goop->bs.liquid_ot.size_int * 4;
    double mem_mb = mem_bytes / 1000000.0;

    char perf_text[256];
    snprintf(perf_text, sizeof(perf_text),
             "%d fps\n%.3f ms\n%d solids\n%d liquids\n%.3f MB", fps,
             actual_delta * 1000.0, goop->bs.solids.count,
             goop->bs.liquids.count, mem_mb);
    text_render(&goop->txtr, perf_text, 32, 32);

    for (int i = 0; i < component_get_count(COMPONENT_TEXT_BOX); i++) {
      EntityComponent *ec = component_get_from_idx(COMPONENT_TEXT_BOX, i);
      TextBox *text_box = (TextBox *)ec->component;
      text_render(&goop->txtr, text_box->text, text_box->pos.X,
                  text_box->pos.Y);
    }

    glfwSwapBuffers(goop->window);
  }
}