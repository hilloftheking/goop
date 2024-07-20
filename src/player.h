#pragma once

#include "HandmadeMath.h"

#include "core.h"
#include "ik.h"

typedef struct Player {
  double shoot_timer;
  double blink_timer;
  HMM_Quat quat;
  HMM_Vec3 vel;
  HMM_Vec3 grav;
  int health;
  IKChain ik_chain;
} Player;

Entity player_create();

void player_process(Entity ent);
