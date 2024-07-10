#include <stdlib.h>
#include <string.h>

#include "core.h"
#include "fixed_array.h"

void fixed_array_create(FixedArray* a, int element_size, int capacity) {
  a->element_size = element_size;
  a->capacity = capacity;
  a->count = 0;

  a->data = alloc_mem(element_size * capacity);
}

void fixed_array_destroy(FixedArray *a) {
  a->capacity = 0;
  a->count = 0;

  a->data = NULL;
}

void *fixed_array_get(FixedArray *a, int idx) {
  return (char *)a->data + idx * a->element_size;
}

const void *fixed_array_get_const(const FixedArray *a, int idx) {
  return (const char *)a->data + idx * a->element_size;
}

int fixed_array_get_idx_from_ptr(const FixedArray *a, const void *element) {
  return (int)((const char *)element - (const char *)a->data) / a->element_size;
}

void *fixed_array_append(FixedArray *a, void *element) {
  if (a->count >= a->capacity) {
    return NULL;
  }

  if (element != NULL) {
    memcpy(fixed_array_get(a, a->count), element, a->element_size);
  }
  return fixed_array_get(a, a->count++);
}

void fixed_array_remove(FixedArray* a, int idx) {
  int last_idx = a->count - 1;
  int cpy_size = (last_idx - idx) * a->element_size;
  memmove(fixed_array_get(a, idx), fixed_array_get(a, idx + 1), cpy_size);
  a->count--;
}
