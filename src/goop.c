#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <glad/glad.h>

#include <GLFW/glfw3.h>

#include "core.h"
#include "ecs.h"
#include "goop.h"
#include "primitives.h"

// These are currently needed to register ECS components
#include "creature.h"
#include "enemies/floater.h"
#include "player.h"

#define WIN_RES_X 1024
#define WIN_RES_Y 768

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

void goop_create(GoopEngine* goop) {
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

void goop_destroy(GoopEngine* goop) {
  skybox_destroy(&goop->skybox);

  blob_sim_destroy(&goop->bs);
  blob_renderer_destroy(&goop->br);

  text_renderer_destroy(&goop->txtr);

  glfwTerminate();
}
