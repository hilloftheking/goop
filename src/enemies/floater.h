#pragma once

#include "core.h"

typedef struct Floater {
  int health;
  double shoot_timer;
} Floater;

Entity floater_create();

void floater_process(Entity ent);