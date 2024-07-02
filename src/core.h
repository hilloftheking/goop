#pragma once

#include <stdint.h>

#define ARR_SIZE(a) (sizeof(a) / sizeof(*a))

typedef uint64_t Entity;
typedef struct BlobSim BlobSim;

typedef struct Global {
  Entity player_ent;
  BlobSim *blob_sim;
  double curr_delta;
} Global;

extern Global global;

void exit_fatal_error();