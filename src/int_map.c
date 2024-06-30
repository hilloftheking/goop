#include <stdlib.h>
#include <stdio.h>

#include "int_map.h"

#define INT_MAP_START_CAPACITY 64

typedef struct IntMapKV {
  uint64_t key;
  uint64_t value;
} IntMapKV;

static void maybe_realloc(IntMap *map) {
  if (!map->data) {
    map->data = malloc(map->capacity * sizeof(IntMapKV));
    for (uint64_t i = 0; i < map->capacity; i++) {
      ((IntMapKV *)map->data)[i].key = -1;
    }
  }
}

void int_map_create(IntMap *map) {
  map->count = 0;
  map->capacity = INT_MAP_START_CAPACITY;
  map->data = NULL;
  maybe_realloc(map);
}

void int_map_destroy(IntMap *map) {
  map->count = 0;
  map->capacity = 0;
  free(map->data);
  map->data = NULL;
}

void int_map_insert(IntMap *map, uint64_t key, uint64_t value) {
  IntMapKV *kv = (IntMapKV *)map->data + (key % map->capacity);
  while (kv->key != -1 && kv->key != key) {
    kv++;
    if (kv >= (IntMapKV *)map->data + map->capacity) {
      kv = map->data;
    }
  }

  kv->value = value;
  if (kv->key == key) {
    return;
  }
  kv->key = key;
  map->count++;
  maybe_realloc(map);
}

uint64_t *int_map_get(IntMap *map, uint64_t key) {
  IntMapKV *kv = (IntMapKV *)map->data + (key % map->capacity);
  while (kv->key != -1 && kv->key != key) {
    kv++;
    if (kv >= (IntMapKV *)map->data + map->capacity) {
      kv = map->data;
    }
  }

  if (kv->key == key) {
    return &kv->value;
  } else {
    fprintf(stderr, "Couldn't find key\n");
    return NULL;
  }
}

void int_map_delete(IntMap *map, uint64_t key) {}
