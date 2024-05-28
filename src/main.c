#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#define _USE_MATH_DEFINES
#include <math.h>

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "linmath.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "blob.h"
#include "solid.h"

static GLuint compile_shader(const char *path, GLenum shader_type) {
  FILE *f = fopen(path, "rb");
  if (!f) {
    // TODO: fallback shader
    fprintf(stderr, "Failed to open file %s\n", path);
    return 0;
  }

  fseek(f, 0, SEEK_END);
  int size = ftell(f);
  fseek(f, 0, SEEK_SET);

  if (size == 0) {
    fprintf(stderr, "File %s is empty\n", path);
    return 0;
  }

  char *content = malloc(size);
  fread(content, 1, size, f);
  fclose(f);

  GLuint shader = glCreateShader(shader_type);
  glShaderSource(shader, 1, &content, &size);
  glCompileShader(shader);
  free(content);

  int compile_status = 0;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &compile_status);

  // Compilation failed
  if (compile_status == GL_FALSE) {
    int log_length = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);

    char *message = malloc(log_length);
    glGetShaderInfoLog(shader, log_length, NULL, message);

    fprintf(stderr, "Shader compilation failed for %s: %s\n", path, message);
    free(message);

    glDeleteShader(shader);
    return 0;
  }

  return shader;
}

static GLuint create_shader_program(const char *vert_shader_path,
                                    const char *frag_shader_path) {
  GLuint vert_shader = compile_shader(vert_shader_path, GL_VERTEX_SHADER);
  if (!vert_shader) {
    // TODO: Better error handling
    return 0;
  }
  GLuint frag_shader = compile_shader(frag_shader_path, GL_FRAGMENT_SHADER);
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

    fprintf(stderr, "Shader linking failed for %s and %s: %s\n",
            vert_shader_path, frag_shader_path, message);
    free(message);

    glDeleteProgram(shader_program);
    return 0;
  }

  return shader_program;
}

static GLuint create_compute_program(const char* shader_path) {
    GLuint shader = compile_shader(shader_path, GL_COMPUTE_SHADER);
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

        char* message = malloc(log_length);
        glGetProgramInfoLog(shader_program, log_length, NULL, message);

        fprintf(stderr, "Shader linking failed for %s: %s\n",
            shader_path, message);
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

#define SWAP_INTERVAL 1

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

int main() {
  if (!glfwInit())
    return -1;

  // OpenGL 4.3 for compute shaders

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  //glfwWindowHint(GLFW_FLOATING, GLFW_TRUE);

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

  glfwMakeContextCurrent(window);
  glfwSwapInterval(SWAP_INTERVAL);

  gladLoadGLLoader(glfwGetProcAddress);

  GLuint vao;
  glGenVertexArrays(1, &vao);
  glBindVertexArray(vao);

  GLuint vbo;
  glGenBuffers(1, &vbo);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);

  GLuint shader_program = create_shader_program("shaders/vert.glsl", "shaders/frag.glsl");
  glUseProgram(shader_program);

  GLint cam_trans_uniform = glGetUniformLocation(shader_program, "cam_trans");
  GLint cam_fov_uniform = glGetUniformLocation(shader_program, "cam_fov");
  GLint cam_aspect_uniform = glGetUniformLocation(shader_program, "cam_aspect");
  glUniform1f(cam_fov_uniform, 60.0f);
  glUniform1f(cam_aspect_uniform, 1.0f);
  cam.pos[1] = 3.0f;
  cam.pos[2] = -5.0f;
  cam.rot[0] = 0.5f;

  GLint blobs_uniform = glGetUniformLocation(shader_program, "blobs");
  GLint solids_uniform = glGetUniformLocation(shader_program, "solids");
  GLint solid_pools_uniform =
      glGetUniformLocation(shader_program, "solid_pools");

  Blob blobs[BLOB_COUNT];
  for (int i = 0; i < BLOB_COUNT; i++) {
    Blob *b = &blobs[i];
    b->pos[0] = (rand_float() - 0.5f) * 5.0f;
    b->pos[1] = 4.0f + (rand_float() * 6.0f);
    b->pos[2] = (rand_float() - 0.5f) * 5.0f;
    b->sleep_ticks = 0;
  }
  vec3 prev_blobs_pos[BLOB_COUNT] = {0};

  vec3 fall_order[] = {
      {0, -1, 0}, {1, -1, 0}, {-1, -1, 0}, {0, -1, 1}, {0, -1, -1}};

  GLint attrib_index = glGetAttribLocation(shader_program, "in_pos");
  glEnableVertexAttribArray(attrib_index);
  glVertexAttribPointer(attrib_index, 2, GL_FLOAT, GL_FALSE, 0, NULL);

  uint64_t timer_freq = glfwGetTimerFrequency();
  uint64_t prev_timer = glfwGetTimerValue();

  int fps = 0;
  int frames_this_second = 0;
  double second_timer = 1.0;

  double tick_timer = 0.0;

  bool running = false;

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

    glUseProgram(shader_program);
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

    if (glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS) {
      running = true;
    }

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

    glUniformMatrix4fv(cam_trans_uniform, 1, GL_FALSE, cam_trans[0]);
    int win_w, win_h;
    glfwGetWindowSize(window, &win_w, &win_h);
    glViewport(0, 0, win_w, win_h);
    glUniform1f(cam_aspect_uniform, (float)win_w / win_h);

    // Simulate blobs every tick

    if (running)
      tick_timer -= delta;

    if (tick_timer <= 0.0) {
      tick_timer = TICK_TIME;

      for (int b = 0; b < BLOB_COUNT; b++) {
        if (blobs[b].sleep_ticks >= BLOB_SLEEP_TICKS_REQUIRED) {
          continue;
        }

        vec3_dup(prev_blobs_pos[b], blobs[b].pos);

        vec3 velocity = {0};
        float attraction_total_magnitude = 0.0f;
        float anti_grav = 0.0f;

        for (int ob = 0; ob < BLOB_COUNT; ob++) {
          if (ob == b)
            continue;

          vec3 attraction;
          blob_get_attraction_to(attraction, &blobs[b], &blobs[ob]);

          float attraction_magnitude = vec3_len(attraction);
          if (attraction_magnitude > BLOB_SLEEP_THRESHOLD) {
            blobs[ob].sleep_ticks = 0;
          }

          attraction_total_magnitude += vec3_len(attraction);
          vec3_add(velocity, velocity, attraction);

          anti_grav += blob_get_support_with(&blobs[b], &blobs[ob]);
        }

        velocity[1] -= BLOB_FALL_SPEED * (1.0f - fminf(anti_grav, 1.0f));

        vec3 pos_before;
        vec3_dup(pos_before, blobs[b].pos);

        vec3_add(blobs[b].pos, blobs[b].pos, velocity);

        for (int i = 0; i < 3; i++) {
          if (blobs[b].pos[i] < blob_min_pos[i]) {
            blobs[b].pos[i] = blob_min_pos[i];
          }
          if (blobs[b].pos[i] > blob_max_pos[i]) {
            blobs[b].pos[i] = blob_max_pos[i];
          }
        }

        // If movement during this tick was under the sleep threshold, the sleep counter can be incremented
        vec3 pos_diff;
        vec3_sub(pos_diff, blobs[b].pos, pos_before);
        float movement = vec3_len(pos_diff);
        if (movement < BLOB_SLEEP_THRESHOLD) {
          blobs[b].sleep_ticks++;
        } else {
          blobs[b].sleep_ticks = 0;
        }
      }
    }

    // Lerp the positions of the blobs

    vec4 blob_lerp[BLOB_COUNT];

    for (int i = 0; i < BLOB_COUNT; i++) {
      float *pos_lerp = blob_lerp[i];
      for (int x = 0; x < 3; x++) {
        float diff = blobs[i].pos[x] - prev_blobs_pos[i][x];
        pos_lerp[x] = prev_blobs_pos[i][x] +
                      (diff * ((TICK_TIME - tick_timer) / TICK_TIME));
      }
    }

    glUniform4fv(blobs_uniform, BLOB_COUNT, blob_lerp);
  }

  glDeleteProgram(shader_program);

  glfwTerminate();
  return 0;
}
