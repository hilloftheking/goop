#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <glad/glad.h>

#include "stb/stb_image.h"

#include "cube.h"
#include "resource.h"
#include "skybox.h"
#include "shader.h"
#include "shader_sources.h"

void skybox_create(Skybox* sb) {
  const int faces[] = {IDB_BLUECLOUD_RT, IDB_BLUECLOUD_LF, IDB_BLUECLOUD_UP,
                       IDB_BLUECLOUD_DN, IDB_BLUECLOUD_BK, IDB_BLUECLOUD_FT};

  glGenTextures(1, &sb->tex);
  glBindTexture(GL_TEXTURE_CUBE_MAP, sb->tex);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

  for (int i = 0; i < 6; i++) {
    HRSRC rsrc = FindResourceA(NULL, MAKEINTRESOURCEA(faces[i]), "JPG");
    if (!rsrc) {
      printf("Failed to load image resource %d\n", faces[i]);
      exit(-1);
    }
    DWORD data_size = SizeofResource(NULL, rsrc);
    HGLOBAL h_data = LoadResource(NULL, rsrc);
    if (!h_data) {
      printf("Failed to load image resource %d\n", faces[i]);
      exit(-1);
    }
    void *data = LockResource(h_data);

    int width, height;
    stbi_uc *pixels =
        stbi_load_from_memory(data, data_size, &width, &height, NULL, 4);
    if (!pixels) {
      printf("Failed to parse image resource %d\n", faces[i]);
      exit(-1);
    }

    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGBA8, width, height,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    stbi_image_free(pixels);

    FreeResource(h_data);
  }

  sb->program = create_shader_program(SKYBOX_VERT_SRC, SKYBOX_FRAG_SRC);
}

void skybox_draw(const Skybox* sb, const mat4x4 view_mat,
    const mat4x4 proj_mat) {
  glDepthMask(GL_FALSE);
  glUseProgram(sb->program);
  glUniformMatrix4fv(0, 1, GL_FALSE, view_mat[0]);
  glUniformMatrix4fv(1, 1, GL_FALSE, proj_mat[0]);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_CUBE_MAP, sb->tex);
  cube_draw();
  glDepthMask(GL_TRUE);
}

void skybox_destroy(Skybox *sb) {
  glDeleteProgram(sb->program);
  glDeleteTextures(1, &sb->tex);
}