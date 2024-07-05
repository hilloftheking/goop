#include <stdio.h>
#include <stdlib.h>

#include <GLFW/glfw3.h>

#include "core.h"

Global global;

void exit_fatal_error() {
  fprintf(stderr, "FATAL ERROR. Press enter to exit\n");

  // If GLFW isn't terminated, the window will be frozen
  glfwTerminate();

  getchar();
  exit(-1);
}

float rand_float() { return ((float)rand() / (float)(RAND_MAX)); }