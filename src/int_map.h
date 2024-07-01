#pragma once

#include <stdint.h>

typedef struct IntMapKV {
  uint64_t key;
  uint64_t value;
} IntMapKV;

typedef struct IntMap {
  int count;
  int capacity;

  IntMapKV *data;
} IntMap;

void int_map_create(IntMap *map);
void int_map_destroy(IntMap *map);

void int_map_insert(IntMap *map, uint64_t key, uint64_t value);
uint64_t *int_map_get(IntMap *map, uint64_t key);
void int_map_delete(IntMap *map, uint64_t key);