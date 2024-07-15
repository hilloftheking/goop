#pragma once

#include "core.h"

typedef struct GoopEngine GoopEngine;

typedef struct Editor {
  GoopEngine *goop;
  int selected;
} Editor;

void editor_init(GoopEngine *goop);

void editor_process(Entity ent);