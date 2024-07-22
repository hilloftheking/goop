#pragma once

#include "HandmadeMath.h"

#include "core.h"

typedef struct GoopEngine GoopEngine;

typedef enum MoveAxis {
  MOVE_X = 1,
  MOVE_Y = 2,
  MOVE_Z = 4,
  MOVE_ALL = 7
} MoveAxis;

typedef struct Editor {
  GoopEngine *goop;
  int state;
  union {
    struct {
      double relx, rely;
      HMM_Vec3 start_pos;
      MoveAxis axis;
    } move;
  };
  int selected;
  char selected_text[128];
  Entity selected_text_box_ent;
} Editor;

void editor_init(GoopEngine *goop);

void editor_process(Entity ent);