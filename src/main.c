#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glad/glad.h>

#include <GLFW/glfw3.h>

#include "linmath.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "blob.h"
#include "blob_render.h"
#include "blob_models.h"
#include "cube.h"
#include "shader.h"
#include "shader_sources.h"

#define ARR_SIZE(a) (sizeof(a) / sizeof(*a))

static void gl_debug_callback(GLenum source, GLenum type, GLuint id,
                              GLenum severity, GLsizei length,
                              const GLchar *message, const void *user_param) {
  fprintf(stderr, "[GL DEBUG] %s\n", message);
}

#define PLR_SPEED 4.0f
#define CAM_SENS 0.01f

static vec2 cam_rot;

static struct {
  BlobRenderer *br;
} cb_data;

static void cursor_position_callback(GLFWwindow *window, double xpos,
                                     double ypos) {
  static double last_xpos = 0.0;
  static double last_ypos = 0.0;

  if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_1) == GLFW_PRESS) {
    cam_rot[0] -= (float)(ypos - last_ypos) * CAM_SENS;
    cam_rot[1] -= (float)(xpos - last_xpos) * CAM_SENS;

    cam_rot[0] = fmodf(cam_rot[0], (float)M_PI * 2.0f);
    cam_rot[1] = fmodf(cam_rot[1], (float)M_PI * 2.0f);
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

static void glfw_fatal_error() {
  const char *err_desc;
  glfwGetError(&err_desc);
  fprintf(stderr, "[GLFW fatal] %s\n", err_desc);
  glfwTerminate();
  exit(-1);
}

int main() {
  if (!glfwInit())
    glfw_fatal_error();

  // OpenGL 4.3 for compute shaders

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
  glfwWindowHint(GLFW_DEPTH_BITS, 32);

  GLFWwindow *window = glfwCreateWindow(512, 512, "goop", NULL, NULL);
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

  BlobSimulation blob_simulation;
  blob_simulation_create(&blob_simulation);

  // Create duck out of ModelBlobs

  int duck_idx = blob_simulation.model_blob_count;
  int duck_count = ARR_SIZE(DUCK_MDL);
  ModelBlob *duck_blobs[ARR_SIZE(DUCK_MDL)];
  vec4 duck_offsets[ARR_SIZE(DUCK_MDL)];

  for (int i = ARR_SIZE(DUCK_MDL) - 1; i >= 0; i--) {
    duck_blobs[i] = model_blob_create(&blob_simulation, DUCK_MDL[i].radius,
                                      DUCK_MDL[i].pos, DUCK_MDL[i].mat_idx);
    vec3_sub(duck_offsets[i], DUCK_MDL[i].pos, DUCK_MDL[0].pos);
    duck_offsets[i][3] = 0.0f;
  }

  // Angry face model

  int angry_idx = blob_simulation.model_blob_count;
  int angry_count = ARR_SIZE(ANGRY_MDL);
  ModelBlob *angry_blobs[ARR_SIZE(ANGRY_MDL)];
  for (int i = 0; i < ARR_SIZE(ANGRY_MDL); i++) {
    angry_blobs[i] = model_blob_create(&blob_simulation, ANGRY_MDL[i].radius,
                                       ANGRY_MDL[i].pos, ANGRY_MDL[i].mat_idx);
  }

  // Create cube buffers for blob renderer and skybox
  cube_create_buffers();

  BlobRenderer blob_renderer;
  blob_renderer_create(&blob_renderer);
  cb_data.br = &blob_renderer;

  cam_rot[0] = 0.5f;
  cam_rot[1] = (float)M_PI;

  GLuint skybox_tex;
  {
    glGenTextures(1, &skybox_tex);
    glBindTexture(GL_TEXTURE_CUBE_MAP, skybox_tex);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    const char prefix[] = "assets/bluecloud_";
    const char *faces[] = {"rt.jpg", "lf.jpg", "up.jpg",
                           "dn.jpg", "bk.jpg", "ft.jpg"};
    for (int i = 0; i < 6; i++) {
      char path[32];
      strncpy(path, prefix, sizeof(path));
      strncat(path, faces[i], sizeof(path) - sizeof(prefix));
      int width, height;
      stbi_uc *data = stbi_load(path, &width, &height, NULL, 4);
      if (!data) {
        printf("Failed to load image %s\n", path);
        exit(-1);
      }

      glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGBA8, width,
                   height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
      stbi_image_free(data);
    }
  }

  GLuint skybox_program =
      create_shader_program(SKYBOX_VERT_SRC, SKYBOX_FRAG_SRC);

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
    snprintf(win_title, sizeof(win_title), "%d fps  -  %lf ms", fps,
             delta * 1000.0);
    glfwSetWindowTitle(window, win_title);

    glfwPollEvents();

    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_1) == GLFW_PRESS)
      glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    else
      glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

    vec4 *cam_trans = blob_renderer.cam_trans;
    mat4x4_identity(cam_trans);

    mat4x4_rotate_Y(cam_trans, cam_trans, cam_rot[1]);
    mat4x4_rotate_X(cam_trans, cam_trans, cam_rot[0]);

    vec3_scale(cam_trans[3], cam_trans[2], 5.0f);
    vec3_add(cam_trans[3], cam_trans[3], duck_blobs[0]->pos);

    // Hold space to spawn more blobs

    if (blob_spawn_cd > 0.0) {
      blob_spawn_cd -= delta;
    }

    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS &&
        blob_spawn_cd <= 0.0) {
      blob_spawn_cd = BLOB_SPAWN_CD;

      vec3 pos;
      vec3_scale(pos, cam_trans[2], -5.0f);
      vec3_add(pos, pos, cam_trans[3]);
      pos[1] += 1.0f;
      blob_create(&blob_simulation, BLOB_LIQUID, 0.5f, pos, 2);
    }

    // Simulate blobs

    if (blob_sim_running)
      blob_simulate(&blob_simulation, delta);

    // Move duck with WASD

    bool w_press = glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS;
    bool s_press = glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS;
    bool a_press = glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS;
    bool d_press = glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS;
    vec4 dir = {0};
    dir[0] = (float)(d_press - a_press);
    dir[2] = (float)(s_press - w_press);
    if (dir[0] || dir[2]) {
      vec4_norm(dir, dir);
      vec4_scale(dir, dir, PLR_SPEED * (float)delta);

      vec4 rel_move;
      mat4x4_mul_vec4(rel_move, cam_trans, dir);

      vec3 test_pos;
      vec3_add(test_pos, duck_blobs[0]->pos, rel_move);

      vec3 correction;
      blob_get_correction_from_solids(correction, &blob_simulation, test_pos,
                                      duck_blobs[0]->radius, false);

      vec3_add(duck_blobs[0]->pos, test_pos, correction);

      mat4x4 rot_mat;
      mat4x4_identity(rot_mat);

      vec3_norm(rot_mat[2], rel_move);
      vec3_scale(rot_mat[2], rot_mat[2], -1.0f);

      vec3_dup(rot_mat[1], cam_trans[1]);

      vec3_mul_cross(rot_mat[0], rot_mat[1], rot_mat[2]);

      for (int i = 0; i < ARR_SIZE(duck_blobs); i++) {
        vec4 p;
        mat4x4_mul_vec4(p, rot_mat, duck_offsets[i]);
        vec3_add(duck_blobs[i]->pos, duck_blobs[0]->pos, p);
      }
    }

    // Render scene

    mat4x4_perspective(blob_renderer.proj_mat, 60.0f * (M_PI / 180.0f),
                       blob_renderer.aspect_ratio, 0.1f, 100.0f);
    mat4x4_invert(blob_renderer.view_mat, cam_trans);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glDepthMask(GL_FALSE);
    glUseProgram(skybox_program);
    glUniformMatrix4fv(0, 1, GL_FALSE, blob_renderer.view_mat[0]);
    glUniformMatrix4fv(1, 1, GL_FALSE, blob_renderer.proj_mat[0]);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, skybox_tex);
    cube_draw();
    glDepthMask(GL_TRUE);

    blob_render_sim(&blob_renderer, &blob_simulation);
    blob_render_mdl(&blob_renderer, &blob_simulation, duck_idx, duck_count);
    blob_render_mdl(&blob_renderer, &blob_simulation, angry_idx, angry_count);
    glfwSwapBuffers(window);
  }

  blob_simulation_destroy(&blob_simulation);
  blob_renderer_destroy(&blob_renderer);

  glfwTerminate();
  return 0;
}
