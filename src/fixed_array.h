#pragma once

typedef struct FixedArray {
  int element_size;
  int capacity;
  int count;
  void *data;
} FixedArray;

void fixed_array_create(FixedArray *a, int element_size, int capacity);
void fixed_array_destroy(FixedArray *a);

// Returns pointer to element at idx
void *fixed_array_get(FixedArray *a, int idx);
const void *fixed_array_get_const(const FixedArray *a, int idx);

// Appends element at the end of the array. Can be NULL. Returns pointer to
// element in array or NULL if failed to append
void *fixed_array_append(FixedArray *a, void *element);

// Removes specified index and resizes the array
void fixed_array_remove(FixedArray *a, int idx);
