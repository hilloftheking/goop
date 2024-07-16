#pragma once

#include <stdbool.h>

#include "blob.h"
#include "blob_render.h"
#include "core.h"
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

typedef enum InputEventType {
  INPUT_KEY = 1,
  INPUT_MOUSE_BUTTON = 2,
  INPUT_MOUSE_MOTION = 4
} InputEventType;

typedef struct InputEvent {
  InputEventType type;
  union {
    struct {
      int key;
      int mods;
      bool pressed;
    } key;
    struct {
      int button;
      bool pressed;
    } mouse_button;
    struct {
      double x, y;
      double relx, rely;
    } mouse_motion;
  };
} InputEvent;

typedef struct InputHandler {
  int mask;
  void (*callback)(Entity ent, InputEvent *);
} InputHandler;

void goop_create(GoopEngine *goop);
void goop_destroy(GoopEngine *goop);

void goop_main_loop(GoopEngine *goop);
