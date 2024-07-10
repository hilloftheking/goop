#include <stdio.h>
#include <stdlib.h>

#include <GLFW/glfw3.h>

#include "core.h"

Global global;

void exit_fatal_error() {
  fprintf(stderr, "FATAL ERROR. Press enter to exit\n");

  // If GLFW isn't terminated, the window will be frozen
  glfwTerminate();

  (void)getchar();
  exit(-1);
}

void *alloc_mem(size_t n) {
  void *mem = malloc(n);
  if (!mem) {
    fprintf(stderr, "Failed to allocate %zu bytes of memory\n", n);
    exit_fatal_error();
    return NULL;
  }

  return mem;
}

void free_mem(void *mem) { free(mem); }

float rand_float() { return ((float)rand() / (float)(RAND_MAX)); }