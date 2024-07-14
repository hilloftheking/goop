#pragma once

#include "blob.h"
#include "blob_render.h"
#include "skybox.h"
#include "text.h"

typedef struct GLFWwindow GLFWwindow;

typedef struct GoopEngine {
  GLFWwindow *window;
  BlobSim bs;
  BlobRenderer br;
  Skybox skybox;
  TextRenderer txtr;
} GoopEngine;

void goop_create(GoopEngine *goop);
void goop_destroy(GoopEngine *goop);