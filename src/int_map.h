#pragma once

#include <stdint.h>

typedef struct IntMap {
  uint64_t count;
  uint64_t capacity;

  void *data;
} IntMap;

void int_map_create(IntMap *map);
void int_map_destroy(IntMap *map);

void int_map_insert(IntMap *map, uint64_t key, uint64_t value);
uint64_t *int_map_get(IntMap *map, uint64_t key);
void int_map_delete(IntMap *map, uint64_t key);