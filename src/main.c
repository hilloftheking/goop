#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define _USE_MATH_DEFINES
#include <math.h>

#include <glad/glad.h>

#include <GLFW/glfw3.h>

#include "linmath.h"

#include "blob.h"
#include "blob_render.h"

static void gl_debug_callback(GLenum source, GLenum type, GLuint id,
                              GLenum severity, GLsizei length,
                              const GLchar *message, const void *user_param) {
  fprintf(stderr, "[GL DEBUG] %s\n", message);
}

#define CAM_SPEED 4.0f
#define CAM_SENS 0.01f

static struct {
  vec3 pos;
  vec2 rot;
} cam;

static struct {
  BlobRenderer *br;
} cb_data;

static void cursor_position_callback(GLFWwindow *window, double xpos,
                                     double ypos) {
  static double last_xpos = 0.0;
  static double last_ypos = 0.0;

  if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_1) == GLFW_PRESS) {
    cam.rot[0] += (float)(ypos - last_ypos) * CAM_SENS;
    cam.rot[1] += (float)(xpos - last_xpos) * CAM_SENS;

    cam.rot[0] = fmodf(cam.rot[0], (float)M_PI * 2.0f);
    cam.rot[1] = fmodf(cam.rot[1], (float)M_PI * 2.0f);
  }

  last_xpos = xpos;
  last_ypos = ypos;
}

static bool blob_sim_running = false;
static bool vsync_enabled = true;

static void key_callback(GLFWwindow *window, int key, int scancode, int action,
                         int mods) {
  if (action != GLFW_PRESS)
    return;

  switch (key) {
  case GLFW_KEY_P:
    blob_sim_running ^= true;
    break;
  case GLFW_KEY_R:
    if (cb_data.br)
      cb_data.br->compute_whole_sdf_this_frame = true;
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
  if (cb_data.br) {
    cb_data.br->aspect_ratio = (float)width / height;
  }
}

int main() {
  if (!glfwInit())
    return -1;

  // OpenGL 4.3 for compute shaders

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

  GLFWwindow *window = glfwCreateWindow(512, 512, "goop", NULL, NULL);
  if (!window) {
    const char *err_desc;
    glfwGetError(&err_desc);
    fprintf(stderr, "Failed to create window: %s\n", err_desc);

    glfwTerminate();
    return -1;
  }

  if (glfwRawMouseMotionSupported())
    glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
  else
    fprintf(stderr, "Raw mouse input not supported\n");

  glfwSetCursorPosCallback(window, cursor_position_callback);
  glfwSetKeyCallback(window, key_callback);
  glfwSetWindowSizeCallback(window, window_size_callback);

  glfwMakeContextCurrent(window);
  glfwSwapInterval((int)vsync_enabled);

  gladLoadGLLoader((void *)(const char *)glfwGetProcAddress);

  glEnable(GL_DEBUG_OUTPUT);
  glDebugMessageCallback(gl_debug_callback, NULL);
  glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE,
                        GL_DEBUG_SEVERITY_NOTIFICATION, 0, NULL, GL_FALSE);

  BlobSimulation blob_simulation;
  blob_simulation_create(&blob_simulation);

  BlobRenderer blob_renderer;
  blob_renderer_create(&blob_renderer);
  cb_data.br = &blob_renderer;

  cam.pos[1] = 3.0f;
  cam.pos[2] = -5.0f;
  cam.rot[0] = 0.5f;

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

    char win_title[100];
    snprintf(win_title, sizeof(win_title), "%d fps  -  %lf ms", fps, delta);
    glfwSetWindowTitle(window, win_title);

    glfwPollEvents();

    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_1) == GLFW_PRESS)
      glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    else
      glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

    mat4x4 cam_trans = {{1.0f, 0.0f, 0.0f, 0.0f},
                        {0.0f, 1.0f, 0.0f, 0.0f},
                        {0.0f, 0.0f, 1.0f, 0.0f},
                        {0.0f, 0.0f, 0.0f, 1.0f}};

    mat4x4_rotate_Y(cam_trans, cam_trans, cam.rot[1]);
    mat4x4_rotate_X(cam_trans, cam_trans, cam.rot[0]);

    bool w_press = glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS;
    bool s_press = glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS;
    bool a_press = glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS;
    bool d_press = glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS;
    vec4 dir = {0};
    dir[0] = (float)(d_press - a_press);
    dir[2] = (float)(w_press - s_press);
    vec4_norm(dir, dir);
    vec4_scale(dir, dir, CAM_SPEED * (float)delta);

    vec4 rel_move;
    mat4x4_mul_vec4(rel_move, cam_trans, dir);
    vec3_add(cam.pos, cam.pos, rel_move);

    cam_trans[3][0] = cam.pos[0];
    cam_trans[3][1] = cam.pos[1];
    cam_trans[3][2] = cam.pos[2];

    mat4x4_dup(blob_renderer.cam_trans, cam_trans);

    // Hold space to spawn more blobs

    if (blob_spawn_cd > 0.0) {
      blob_spawn_cd -= delta;
    }

    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS &&
        blob_spawn_cd <= 0.0) {
      blob_spawn_cd = BLOB_SPAWN_CD;

      vec3 pos;
      vec3_scale(pos, cam_trans[2], 4.0f);
      vec3_add(pos, pos, cam_trans[3]);
      blob_create(&blob_simulation, pos);
    }

    // Simulate blobs

    if (blob_sim_running)
      blob_simulate(&blob_simulation, delta);

    // Render blobs

    glClear(GL_COLOR_BUFFER_BIT);
    blob_render(&blob_renderer, &blob_simulation);
    glfwSwapBuffers(window);
  }

  blob_simulation_destroy(&blob_simulation);
  blob_renderer_destroy(&blob_renderer);

  glfwTerminate();
  return 0;
}
