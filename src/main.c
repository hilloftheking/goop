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

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "blob.h"

#include "shader_sources.h"

static GLuint compile_shader(const char *source, GLenum shader_type) {
  int size = strlen(source);

  GLuint shader = glCreateShader(shader_type);
  glShaderSource(shader, 1, &source, &size);
  glCompileShader(shader);

  int compile_status = 0;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &compile_status);

  // Compilation failed
  // TODO: fallback shader
  if (compile_status == GL_FALSE) {
    int log_length = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);

    char *message = malloc(log_length);
    glGetShaderInfoLog(shader, log_length, NULL, message);

    fprintf(stderr, "Shader compilation failed: %s\n", message);
    free(message);

    glDeleteShader(shader);
    return 0;
  }

  return shader;
}

static GLuint create_shader_program(const char *vert_shader_src,
                                    const char *frag_shader_src) {
  GLuint vert_shader = compile_shader(vert_shader_src, GL_VERTEX_SHADER);
  if (!vert_shader) {
    // TODO: Better error handling
    return 0;
  }
  GLuint frag_shader = compile_shader(frag_shader_src, GL_FRAGMENT_SHADER);
  if (!frag_shader) {
    return 0;
  }

  GLuint shader_program = glCreateProgram();
  glAttachShader(shader_program, vert_shader);
  glAttachShader(shader_program, frag_shader);
  // Shader objects only have to exist for this call
  glLinkProgram(shader_program);

  glDetachShader(shader_program, vert_shader);
  glDetachShader(shader_program, frag_shader);
  glDeleteShader(vert_shader);
  glDeleteShader(frag_shader);

  int link_status = 0;
  glGetProgramiv(shader_program, GL_LINK_STATUS, &link_status);

  // Linking failed
  if (link_status == GL_FALSE) {
    int log_length = 0;
    glGetProgramiv(shader_program, GL_INFO_LOG_LENGTH, &log_length);

    char *message = malloc(log_length);
    glGetProgramInfoLog(shader_program, log_length, NULL, message);

    fprintf(stderr, "Shader linking: %s\n", message);
    free(message);

    glDeleteProgram(shader_program);
    return 0;
  }

  return shader_program;
}

static GLuint create_compute_program(const char *source) {
  GLuint shader = compile_shader(source, GL_COMPUTE_SHADER);
  if (!shader) {
    // TODO: Better error handling
    return 0;
  }

  GLuint shader_program = glCreateProgram();
  glAttachShader(shader_program, shader);
  // Shader objects only have to exist for this call
  glLinkProgram(shader_program);

  glDetachShader(shader_program, shader);
  glDeleteShader(shader);

  int link_status = 0;
  glGetProgramiv(shader_program, GL_LINK_STATUS, &link_status);

  // Linking failed
  if (link_status == GL_FALSE) {
    int log_length = 0;
    glGetProgramiv(shader_program, GL_INFO_LOG_LENGTH, &log_length);

    char *message = malloc(log_length);
    glGetProgramInfoLog(shader_program, log_length, NULL, message);

    fprintf(stderr, "Shader linking: %s\n", message);
    free(message);

    glDeleteProgram(shader_program);
    return 0;
  }

  return shader_program;
}

static float rand_float() { return ((float)rand() / (float)(RAND_MAX)); }

static vec2 verts[] = {{-1, 1}, {1, 1}, {-1, -1}, {1, -1}, {-1, -1}, {1, 1}};

static struct {
  vec3 pos;
  vec2 rot;
} cam;

#define TICK_TIME 0.1
#define CAM_SPEED 2.0f
#define CAM_SENS 0.01f

static void cursor_position_callback(GLFWwindow *window, double xpos,
                                     double ypos) {
  static double last_xpos = 0.0;
  static double last_ypos = 0.0;

  if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_1) == GLFW_PRESS) {
    cam.rot[0] += (ypos - last_ypos) * CAM_SENS;
    cam.rot[1] += (xpos - last_xpos) * CAM_SENS;

    cam.rot[0] = fmodf(cam.rot[0], M_PI * 2.0f);
    cam.rot[1] = fmodf(cam.rot[1], M_PI * 2.0f);
  }

  last_xpos = xpos;
  last_ypos = ypos;
}

static bool blob_sim_running = false;

// If true, then recalculate the entire SDF image this frame
static bool compute_whole_sdf_this_frame = true;

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
    compute_whole_sdf_this_frame = true;
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
  // Hopefully the correct shader is being used kek
  glUniform1f(3, (float)width / height);
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

  gladLoadGLLoader(glfwGetProcAddress);

  GLuint vao;
  glGenVertexArrays(1, &vao);
  glBindVertexArray(vao);

  GLuint vbo;
  glGenBuffers(1, &vbo);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);

  GLuint raymarch_program =
      create_shader_program(RAYMARCH_VERT_SRC, RAYMARCH_FRAG_SRC);
  glUseProgram(raymarch_program);

  glUniform1f(2, 60.0f);
  glUniform1f(3, 1.0f);
  cam.pos[1] = 3.0f;
  cam.pos[2] = -5.0f;
  cam.rot[0] = 0.5f;

  Blob *blobs = malloc(BLOB_COUNT * sizeof(Blob));
  for (int i = 0; i < BLOB_COUNT; i++) {
    Blob *b = &blobs[i];
    b->pos[0] = (rand_float() - 0.5f) * 5.0f;
    b->pos[1] = 4.0f + (rand_float() * 6.0f);
    b->pos[2] = (rand_float() - 0.5f) * 5.0f;
    b->sleep_ticks = 0;
  }

  vec3 *blobs_prev_pos = calloc(BLOB_COUNT, sizeof(vec3));
  vec4 *blob_lerp = calloc(BLOB_COUNT, sizeof(vec4));

  vec3 fall_order[] = {
      {0, -1, 0}, {1, -1, 0}, {-1, -1, 0}, {0, -1, 1}, {0, -1, -1}};

  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);

  GLuint sdf_tex;
  glGenTextures(1, &sdf_tex);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_3D, sdf_tex);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexImage3D(GL_TEXTURE_3D, 0, GL_R32F, BLOB_SDF_RES, BLOB_SDF_RES,
               BLOB_SDF_RES, 0, GL_RED, GL_FLOAT, NULL);
  glBindImageTexture(0, sdf_tex, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32F);

  GLuint blobs_tex;
  glGenTextures(1, &blobs_tex);
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_1D, blobs_tex);
  glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexImage1D(GL_TEXTURE_1D, 0, GL_RGBA32F, BLOB_COUNT, 0, GL_RGBA, GL_FLOAT,
               NULL);
  glBindImageTexture(1, blobs_tex, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA32F);

  GLuint compute_program = create_compute_program(COMPUTE_SDF_COMP_SRC);
  glUseProgram(compute_program);
  glUniform1i(2, BLOB_COUNT);

  uint64_t timer_freq = glfwGetTimerFrequency();
  uint64_t prev_timer = glfwGetTimerValue();

  int fps = 0;
  int frames_this_second = 0;
  double second_timer = 1.0;

  double tick_timer = 0.0;

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

    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(raymarch_program);
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    glfwSwapBuffers(window);
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
    dir[0] = (d_press - a_press);
    dir[2] = (w_press - s_press);
    vec4_norm(dir, dir);
    vec4_scale(dir, dir, CAM_SPEED * delta);

    vec4 rel_move;
    mat4x4_mul_vec4(rel_move, cam_trans, dir);
    vec3_add(cam.pos, cam.pos, rel_move);

    cam_trans[3][0] = cam.pos[0];
    cam_trans[3][1] = cam.pos[1];
    cam_trans[3][2] = cam.pos[2];

    glUniformMatrix4fv(1, 1, GL_FALSE, cam_trans[0]);

    // Simulate blobs every tick

    if (blob_sim_running)
      tick_timer -= delta;

    if (tick_timer <= 0.0) {
      tick_timer = TICK_TIME;

      simulate_blobs(blobs, BLOB_COUNT, blobs_prev_pos);
    }

    // Lerp the positions of the blobs

    // Keep track of the minimum and maximum position of blobs so less of the
    // SDF has to be updated
    vec3 min_pos = {1000, 1000, 1000};
    vec3 max_pos = {-1000, -1000, -1000};

    for (int i = 0; i < BLOB_COUNT; i++) {
      float *pos_lerp = blob_lerp[i];
      for (int x = 0; x < 3; x++) {
        float diff = blobs[i].pos[x] - blobs_prev_pos[i][x];
        pos_lerp[x] = blobs_prev_pos[i][x] +
                      (diff * ((TICK_TIME - tick_timer) / TICK_TIME));

        if (pos_lerp[x] < min_pos[x]) {
          min_pos[x] = pos_lerp[x];
        }
        if (pos_lerp[x] > max_pos[x]) {
          max_pos[x] = pos_lerp[x];
        }
      }
    }

    int num_groups[3];

    for (int i = 0; i < 3; i++) {
      min_pos[i] = fmaxf(
          0.0f, floorf(min_pos[i] - blob_sdf_start[i] - BLOB_SDF_MAX_DIST));
      max_pos[i] = fmaxf(
          0.0f, ceilf(max_pos[i] - blob_sdf_start[i] + BLOB_SDF_MAX_DIST));

      num_groups[i] = (int)ceilf(
          ((max_pos[i] - min_pos[i]) * (BLOB_SDF_RES / blob_sdf_size[i])) /
          (float)BLOB_SDF_LOCAL_GROUPS);
      num_groups[i] =
          max(0, min(BLOB_SDF_RES / BLOB_SDF_LOCAL_GROUPS, num_groups[i]));
    }

    glTexSubImage1D(GL_TEXTURE_1D, 0, 0, BLOB_COUNT, GL_RGBA, GL_FLOAT,
                    blob_lerp);

    glUseProgram(compute_program);
    if (compute_whole_sdf_this_frame) {
      compute_whole_sdf_this_frame = false;
      glUniform3i(3, 0, 0, 0);
      glDispatchCompute(BLOB_SDF_RES / BLOB_SDF_LOCAL_GROUPS,
                        BLOB_SDF_RES / BLOB_SDF_LOCAL_GROUPS,
                        BLOB_SDF_RES / BLOB_SDF_LOCAL_GROUPS);
    } else {
      glUniform3i(3, (int)min_pos[0], (int)min_pos[1], (int)min_pos[2]);
      glDispatchCompute(num_groups[0], num_groups[1], num_groups[2]);
    }
  }

  glfwTerminate();
  return 0;
}
