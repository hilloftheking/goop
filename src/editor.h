#pragma once

#include "core.h"

typedef struct GoopEngine GoopEngine;

typedef struct Editor {
  GoopEngine *goop;
  int state;
  int selected;
  char selected_text[128];
  Entity selected_text_box_ent;
} Editor;

void editor_init(GoopEngine *goop);

void editor_process(Entity ent);