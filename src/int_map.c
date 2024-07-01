#include <stdlib.h>
#include <stdio.h>

#include "int_map.h"

#define INT_MAP_START_CAPACITY 32

static void int_map_alloc_data(IntMap *map) {
  map->data = malloc(map->capacity * sizeof(*map->data));
  for (int i = 0; i < map->capacity; i++) {
    map->data[i].key = -1;
  }
}

// Does a linear probe for key until it finds the key or an empty key
static IntMapKV *int_map_find_kv(IntMap *map, uint64_t key) {
  IntMapKV *kv = map->data + (key % map->capacity);
  for (int i = 0; i < map->capacity; i++) {
    if (kv->key == -1 || kv->key == key) {
      break;
    }

    kv++;
    if (kv >= map->data + map->capacity) {
      kv = map->data;
    }
  }

  return kv;
}

static void int_map_insert_no_realloc(IntMap *map, uint64_t key,
                                      uint64_t value) {
  IntMapKV *kv = int_map_find_kv(map, key);

  kv->value = value;
  if (kv->key == key) {
    return;
  }
  kv->key = key;
  map->count++;
}

static void int_map_maybe_realloc(IntMap *map) {
  int new_capacity;
  if (map->count > map->capacity / 2) {
    new_capacity = map->capacity * 2;
  } else if (map->capacity > INT_MAP_START_CAPACITY &&
             map->count < map->capacity / 4) {
    new_capacity = map->capacity / 2;
  } else {
    return;
  }

  IntMap new_map;
  new_map.capacity = new_capacity;
  new_map.count = 0;
  int_map_alloc_data(&new_map);

  // Insert all old key/values
  for (int i = 0; i < map->capacity; i++) {
    IntMapKV *kv = &map->data[i];
    if (kv->key != -1) {
      int_map_insert_no_realloc(&new_map, kv->key, kv->value);
    }
  }

  int_map_destroy(map);
  *map = new_map;
}

void int_map_create(IntMap *map) {
  map->count = 0;
  map->capacity = INT_MAP_START_CAPACITY;
  int_map_alloc_data(map);
}

void int_map_destroy(IntMap *map) {
  map->count = 0;
  map->capacity = 0;
  free(map->data);
  map->data = NULL;
}

void int_map_insert(IntMap *map, uint64_t key, uint64_t value) {
  int_map_insert_no_realloc(map, key, value);
  int_map_maybe_realloc(map);
}

uint64_t *int_map_get(IntMap *map, uint64_t key) {
  IntMapKV *kv = int_map_find_kv(map, key);

  if (kv->key == key) {
    return &kv->value;
  } else {
    return NULL;
  }
}

void int_map_delete(IntMap *map, uint64_t key) {
  IntMapKV *kv = int_map_find_kv(map, key);

  if (kv->key != key) {
    fprintf(stderr, "Couldn't find key to delete\n");
    return;
  }

  kv->key = -1;
  map->count--;

  // Reinsert everything after what just got deleted until an empty key is found
  // or the entire table has been traversed
  for (int i = 0; i < map->capacity - 1; i++) {
    kv++;
    if (kv >= map->data + map->capacity) {
      kv = map->data;
    }

    if (kv->key == -1) {
      break;
    }

    uint64_t key = kv->key;
    uint64_t value = kv->value;
    kv->key = -1;
    map->count--;

    int_map_insert_no_realloc(map, key, value);
  }

  int_map_maybe_realloc(map);
}
