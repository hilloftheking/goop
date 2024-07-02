#pragma once

#include "core.h"

typedef struct Floater {
  double shoot_timer;
} Floater;

Entity floater_create();

void floater_process(Entity ent);